#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/core/spectrum.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;
NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class particle2d final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    particle2d(const Properties &props) : Base(props) {
        mie = props.texture<Texture>("mie", .5f);
        sk = props.texture<Texture>("structure_factor", .5f);
        t_coh = props.texture<Texture>("t_coh", .5f);
        normalization = props.texture<Texture>("normalization", .5f);
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
        naive = props.get<int>("naive",0);
        f = props.get<float>("f",0.5);
    }
    void traverse(TraversalCallback *callback) override {
        callback->put_object("mie", mie.get(), +ParamFlags::Differentiable);
        callback->put_object("structure_factor", sk.get(), +ParamFlags::Differentiable);
        callback->put_object("t_coh", t_coh.get(), +ParamFlags::Differentiable);
        callback->put_object("norm", normalization.get(), +ParamFlags::Differentiable);
    }
    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, 
                                             const SurfaceInteraction3f &si, 
                                             Float sample1, 
                                             const Point2f &sample2, 
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);
        BSDFSample3f bs = dr::zeros<BSDFSample3f>();
        if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active)))
            return { bs, 0.f };
        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));
        auto wo  = warp::square_to_uniform_sphere(sample2);
        auto pdf = warp::square_to_uniform_sphere_pdf(wo);
        Point3f difference = wo+si.wi;
        Point2f projection(difference[0],difference[1]);
        Float d = dr::sqrt(projection[0]*projection[0]+projection[1]*projection[1]);
        Float cos_theta_diff  =dr::dot(wo,-si.wi);

        SurfaceInteraction3f si_mie = si;
        SurfaceInteraction3f si_sk = si;
        SurfaceInteraction3f si_norm = si;
        SurfaceInteraction3f si_T = si;

        Point2f uv_mie((cos_theta_diff+1)/2,0);
        // Point2f uv_mie(d/2,0);
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
        // Spectrum value_T(0.0);
        // Float fraction_T(0.0);
        Spectrum value_T = t_coh->eval(si_T, active);
        Float fraction_T = t_coh->eval_1(si_T, active);
        // std::cout<<fraction_T<<std::endl;
        Mask selected_coh_inc = sample1<=fraction_T;
        Spectrum value_scatter = value_mie*value_sk / value_norm / pdf;


        Spectrum value = dr::select(selected_coh_inc, value_T, value_scatter);
        bs.pdf = dr::select(selected_coh_inc, fraction_T, (1-fraction_T)*pdf);
        bs.wo = dr::select(selected_coh_inc, -si.wi, wo);

        Mask back = dr::dot(-si.wi,bs.wo)<0;
        dr::masked(bs.sampled_type, selected_coh_inc&back) = +BSDFFlags::DeltaReflection;
        dr::masked(bs.sampled_type, selected_coh_inc&!back) = +BSDFFlags::DeltaTransmission;
        dr::masked(bs.sampled_type, !selected_coh_inc&back) = +BSDFFlags::GlossyReflection;
        dr::masked(bs.sampled_type, !selected_coh_inc&!back) = +BSDFFlags::GlossyTransmission;
        // value[1] *=0;
        // value[2] *=0;
        return { bs,value & (active && bs.pdf > 0.f) }; // 返回值示例
    }
    // std::pair<BSDFSample3f, Spectrum> sample_more(const BSDFContext &ctx,
    //                                          const SurfaceInteraction3f &si,
    //                                          Float sample1,
    //                                          const Point2f &sample2,
    //                                          Float /* sample11 */,
    //                                          const Point2f &sample22,
    //                                          Mask active) const override {
    //     MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);


    //     return sample(ctx,si,sample1,sample2,active);
    // }
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
        
        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));
        
        Float cos_theta_diff = dr::dot(-si.wi,wo);
        Point3f difference = wo+si.wi;
        Point2f projection(difference[0],difference[1]);
        Float d = dr::sqrt(projection[0]*projection[0]+projection[1]*projection[1]);
        SurfaceInteraction3f si_mie = si;
        SurfaceInteraction3f si_sk = si;
        SurfaceInteraction3f si_norm = si;
        SurfaceInteraction3f si_T = si;
        Point2f uv_mie((cos_theta_diff+1)/2,0);
        // Point2f uv_mie(d/2,0);
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
        Spectrum value_scatter =   value_mie*value_sk / value_norm*(1-fraction_T);
        
        auto pdf = warp::square_to_uniform_sphere_pdf(wo)*(1-fraction_T);
        // std::cout<<cos_theta_diff<<std::endl;
        // value_scatter[1] *=0;
        // value_scatter[2] *=0;
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
    ref<Texture> mie;
    ref<Texture> sk;
    ref<Texture> t_coh;
    ref<Texture> normalization;
    // ref<Texture> r_coh;
    ScalarFloat ext_ior, int_ior;
    int naive = 0;
    
    // float area;
};

MI_IMPLEMENT_CLASS_VARIANT(particle2d, BSDF)
MI_EXPORT_PLUGIN(particle2d, "Particle 2D")
NAMESPACE_END(mitsuba)
