#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/distr_1d.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <filesystem>
#include <mitsuba/render/sampler.h>
#include <mitsuba/render/phase.h>
namespace fs = std::filesystem;
NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class particle_importance_sampling final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture,Sampler, PhaseFunctionContext)

    particle_importance_sampling(const Properties &props) : Base(props) {
        mie = props.texture<Texture>("mie", .5f);
        sk = props.texture<Texture>("structure_factor", .5f);
        t_coh = props.texture<Texture>("t_coh", .5f);
        normalization = props.texture<Texture>("normalization", .5f);
        
        if (props.type("values") == Properties::Type::String) {
            std::vector<std::string> values_str =
                string::tokenize(props.string("values"), " ,");
            std::vector<ScalarFloat> data;
            data.reserve(values_str.size());

            for (const auto &s : values_str) {
                try {
                    data.push_back((ScalarFloat) std::stod(s));
                } catch (...) {
                    Throw("Could not parse floating point value '%s'", s);
                }
            }

            m_distr_sk_I = ContinuousDistribution<Float>(ScalarVector2f(0.f, 1.f),
                                                    data.data(), data.size());
        } else {
            Throw("'values' must be a string");
        }

        // mie_1d = 
        // r_coh = props.texture<Texture>("r_coh", .5f);

        // std::cout<<"hdshdsf"<<std::endl;
        int_ior = lookup_ior(props, "int_ior", "bk7");

        // Specifies the external index of refraction at the interface
        ext_ior = lookup_ior(props, "ext_ior", "air");

        m_components.push_back(BSDFFlags::DeltaReflection|BSDFFlags::GlossyReflection | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide);
        m_components.push_back(BSDFFlags::DeltaTransmission|BSDFFlags::GlossyTransmission | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide);

        m_flags = m_components[0] | m_components[1];
        m_eta = int_ior / ext_ior;
        radius = props.get<float>("radius",0.5);
        fraction = props.get<float>("f", 0.5);
        // scale = props.get<float>("scale",1);
        // naive = props.get<int>("naive",0);
        f = props.get<float>("f",0.5);
    }
    void traverse(TraversalCallback *callback) override {
        callback->put_object("mie", mie.get(), +ParamFlags::Differentiable);
        callback->put_object("structure_factor", sk.get(), +ParamFlags::Differentiable);
        callback->put_object("t_coh", t_coh.get(), +ParamFlags::Differentiable);
        callback->put_object("norm", normalization.get(), +ParamFlags::Differentiable);
        // callback->put_parameter("values", m_distr_sk.pdf(), ParamFlags::Differentiable | ParamFlags::Discontinuous);
    }
  
    // dummy function
    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, 
                                             const SurfaceInteraction3f &si, 
                                             Float sample1, 
                                             const Point2f &sample2, 
                                             Mask active) const override {
                                                return sample_more(ctx, si, sample1, sample2, sample1, sample2, active);
                                             }
    
    // overrides sample_more
    std::pair<BSDFSample3f, Spectrum> sample_more(const BSDFContext &ctx, 
                                             const SurfaceInteraction3f &si, 
                                             Float sample1, 
                                             const Point2f &sample2, 
                                             Float sample11, 
                                             const Point2f &sample22, 
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);
        BSDFSample3f bs = dr::zeros<BSDFSample3f>();
        if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active)))
            return { bs, 0.f };
        

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));

        Float dmin(0.0);
        // Float dmax(2);
        Float dmax = dr::abs(Frame3f::sin_theta(si.wi))+1;
        // Float pdf_d(0);
        // Point2f pos_d(0);

        Float cdf_min = m_distr_sk_I.eval_cdf_normalized(dmin/2, active);
        Float cdf_max = m_distr_sk_I.eval_cdf_normalized(dmax/2, active);
        auto [pos_d, pdf_d] = m_distr_sk_I.sample_pdf(cdf_min + sample1 * (cdf_max - cdf_min), active);
        pdf_d = pdf_d/(cdf_max - cdf_min)*0.5;
        Float d = pos_d*2;
        // Float d = pos_d*2;

        // Float d = sample1*(dmax-dmin);
        // Float pdf_d = 1/(dmax-dmin);
        Float beta = sample11*dr::Pi<ScalarFloat>*2;
        // 
        // //test with uniformly sample on the circle for outgoing dir
        // Float d_test = sample1;
        // Float beta = sample11*2*M_PI;
        // Float xo = dr::sin(beta)*d_test;
        // Float yo = dr::cos(beta)*d_test;
        // Float zo_ =dr::safe_sqrt(1-d_test*d_test);
        // Float zo = dr::select(sample2[0]<0.5, zo_,-zo_);
        // Point3f difference = wo+si.wi;
        // Point2f projection(difference[0],difference[1]);
        // Float d = dr::sqrt(projection[0]*projection[0]+projection[1]*projection[1]);
        // Vector3f wo = {xo, yo, zo};
        
        Float xo = -si.wi.x()+d*dr::sin(beta);
        Float yo = -si.wi.y()+d*dr::cos(beta);

        Float xo2yo2 = xo*xo+yo*yo;
        Mask valid = xo2yo2<1;
        Float zo_ =dr::safe_sqrt(1-xo*xo-yo*yo);
        Float zo = dr::select(sample2[0]<0.5, zo_,-zo_);
        Vector3f wo = {xo, yo, zo};
        
        Float cos_theta_diff = dr::dot(wo, -si.wi);
        Float sinbeta = dr::sin(beta);
        Float cosbeta = dr::cos(beta);
        Float xi = si.wi[0];
        Float yi = si.wi[1];
        Float zi = si.wi[2];

        // Float J = d_test/zo_/sinbeta/sinbeta/(yo*yo/xo/xo+1);

        Float J = d/zo_;
        Float pdf = dr::select(valid, (pdf_d)*1/dr::Pi<ScalarFloat>/J/2/2, 0);
        
        // Float pdf = 1/M_PI/J/2/2;
        // Float pdf = 1/M_PI;
        SurfaceInteraction3f si_mie = si;
        SurfaceInteraction3f si_sk = si;
        SurfaceInteraction3f si_norm = si;
        SurfaceInteraction3f si_T = si;

        Point2f uv_mie((cos_theta_diff+1)/2,0);
        Point2f uv_sk(d/2,0);
        Point2f uv_norm(cos_theta_i,0);
        Point2f uv_T(cos_theta_i,0);
        si_mie.uv = uv_mie;
        si_sk.uv = uv_sk;
        si_norm.uv = uv_norm;
        si_T.uv = uv_T;

        Spectrum value_mie = mie->eval(si_mie, active);
        Spectrum value_sk = sk->eval(si_sk, active);
        Spectrum value_norm = normalization->eval(si_norm, active);
        Spectrum value_T = t_coh->eval(si_T, active);
        Float fraction_T = t_coh->eval_1(si_T, active);
        // Float pdf(1);
        Mask selected_coh_inc = sample22[0]<=fraction_T;
        Spectrum value_scatter = value_mie * value_sk / value_norm / pdf;
        // Spectrum value_scatter = dr::select(valid, value_mie  / value_norm / pdf, 0);

        
        Spectrum value = dr::select(selected_coh_inc, value_T, value_scatter);
        bs.pdf = dr::select(selected_coh_inc, fraction_T, (1-fraction_T)*pdf);
        bs.wo = dr::select(selected_coh_inc, -si.wi, wo);
        // bs.pdf = 1;
        Mask back = dr::dot(-si.wi,bs.wo)<0;
        dr::masked(bs.sampled_type, selected_coh_inc&back) = +BSDFFlags::DeltaReflection;
        dr::masked(bs.sampled_type, selected_coh_inc&!back) = +BSDFFlags::DeltaTransmission;
        dr::masked(bs.sampled_type, !selected_coh_inc&back) = +BSDFFlags::GlossyReflection;
        dr::masked(bs.sampled_type, !selected_coh_inc&!back) = +BSDFFlags::GlossyTransmission;

        
        
        // bs.wo  = warp::square_to_uniform_sphere(sample2);
        // Spectrum value(1); 
        // std::cout<<value<<std::endl;

        return { bs,value & (active && bs.pdf > 0.f) }; // 返回值示例
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo, Mask active) const override {
        // C++ 实现评估逻辑
        return Spectrum(0.0f); // 返回值示例
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo, Mask active) const override {
        // C++ 实现PDF逻辑
        return 0.0f; // 返回值示例
    }
    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);
        if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active)))
            return { 0.f, 0.f };
        
        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        Float dmin(0.0);
        Float dmax = dr::abs(Frame3f::sin_theta(si.wi))+1;
        //  = dr::abs(Frame3f::sin_theta(si.wi))+1;
        Float xo_diff = (-si.wi+wo)[0];
        Float yo_diff = (-si.wi+wo)[1];
        Float d = dr::safe_sqrt(xo_diff*xo_diff+yo_diff*yo_diff);
        // m_distr_sk_I->eval_pdf_normalized_interval
        Float cdf_min = m_distr_sk_I.eval_cdf_normalized(dmin/2, active);
        Float cdf_max = m_distr_sk_I.eval_cdf_normalized(dmax/2, active);
        Float pdf_d = m_distr_sk_I.eval_pdf_normalized(d/2, active)/(cdf_max - cdf_min)*0.5;
        // Float theta_diff = dr::dot(wo, -si.wi);
        Float cos_theta_diff = dr::dot(wo, -si.wi);

        
        SurfaceInteraction3f si_mie = si;
        SurfaceInteraction3f si_sk = si;
        SurfaceInteraction3f si_norm = si;
        SurfaceInteraction3f si_T = si;
        Point2f uv_mie((cos_theta_diff+1)/2,0);
        Point2f uv_sk(d/2,0);
        Point2f uv_norm(cos_theta_i,0);
        Point2f uv_T(cos_theta_i,0);
        si_mie.uv = uv_mie;
        si_sk.uv = uv_sk;
        si_norm.uv = uv_norm;
        si_T.uv = uv_T;
        Spectrum value_mie = mie->eval(si_mie, active);
        Spectrum value_sk = sk->eval(si_sk, active);
        Spectrum value_norm = normalization->eval(si_norm, active);
        Spectrum value_T = t_coh->eval(si_T, active);

        Float fraction_T = t_coh->eval_1(si_T, active);
        Spectrum value_scatter = value_mie  * value_sk/ value_norm*(1-fraction_T);
        Float sinbeta = (si.wi+wo)[0]/d;
        Float cosbeta = (si.wi+wo)[1]/d;
        Float xi = si.wi[0];
        Float yi = si.wi[1];
        Float zi = si.wi[2];
       
        // Float beta = dr::asin(xi);
        // Float J =dr::abs(dr::square(dr::sin(beta))*dr::safe_sqrt(xi*xi+yi*yi)/wo.z()/dr::square(dr::cos(beta)));
        Float J = d/wo[2];
        // auto pdf = (pdf_d/2)*1/M_PI/J*(1-fraction_T)/2;
        // std::cout<<J<<std::endl;
        Float pdf = pdf_d*1/dr::Pi<ScalarFloat>/2/2/J;
        // value_scatter = 1;
        return { value_scatter&active, dr::select(active, pdf, 0.f)};
    }

    // void traverse(TraversalCallback *callback) override {
    //     // C++ 实现遍历逻辑
    // }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "particle_2d[" << std::endl
            << "  eta = " << m_eta << "," << std::endl
            << "  radius = " << radius << "," << std::endl
            << "  fraction = " << fraction << std::endl
            << "]";
        return oss.str();
    }
    MI_DECLARE_CLASS()
private:
    float m_eta, radius, fraction, scale, f;
    ContinuousDistribution<Float> m_distr_sk_I;
    ref<Texture> mie;
    ref<Texture> sk;
    ref<Texture> t_coh;
    ref<Texture> normalization;
    // ref<Texture> r_coh;
    ScalarFloat ext_ior, int_ior;
    int naive = 0;
    
    // float area;
};

MI_IMPLEMENT_CLASS_VARIANT(particle_importance_sampling, BSDF)
MI_EXPORT_PLUGIN(particle_importance_sampling, "particle_importance_sampling")
NAMESPACE_END(mitsuba)
