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
#include <mitsuba/core/distr_2d.h>
#include <mitsuba/core/distr_1d.h>
namespace fs = std::filesystem;
NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class particle2d_IS_texture final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    particle2d_IS_texture(const Properties &props) : Base(props) {
        m_reflectance = props.texture<Texture>("reflectance", .5f);
        sk = props.texture<Texture>("sk", 1.f);
        I_a = props.texture<Texture>("Ia", 1.f);
// bk7

        int_ior = lookup_ior(props, "int_ior", "bk7");

        // Specifies the external index of refraction at the interface
        ext_ior = lookup_ior(props, "ext_ior", "air");

        m_components.push_back(BSDFFlags::DeltaReflection|BSDFFlags::GlossyReflection | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide);
        m_components.push_back(BSDFFlags::DeltaTransmission|BSDFFlags::GlossyTransmission | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide);

        m_flags = m_components[0] | m_components[1];
        m_eta = int_ior / ext_ior;
        radius = props.get<float>("radius",5);
        fraction = props.get<float>("f", 0.5);
        scale = props.get<float>("scale",1);
        naive = props.get<int>("naive",0);
        sk_I_seperate = props.get<int>("sk_I_seperate",0);
        sample_sk_I_seperate = props.get<int>("sample_sk_I_seperate",0);
        // dielectric_vs_scatter = props.get<float>("vs",1);
        f = props.get<float>("f",0.5);
        counter=1;
        totalcounter=1;


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

            m_distr_sk = ContinuousDistribution<Float>(ScalarVector2f(0.f, 1.f),
                                                    data.data(), data.size());
        } else {
            Throw("'values' must be a string");
        }
       
    }
    void traverse(TraversalCallback *callback) override {
        callback->put_object("reflectance", m_reflectance.get(), +ParamFlags::Differentiable);
        callback->put_object("Ia", I_a.get(), +ParamFlags::Differentiable);
        callback->put_object("sk", sk.get(), +ParamFlags::Differentiable);
    }
    // std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, 
    //                                          const SurfaceInteraction3f &si, 
    //                                          Float sample1, 
    //                                          const Point2f &sample2, 
    //                                          Mask active) const override {
    //     MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);
    //     BSDFSample3f bs = dr::zeros<BSDFSample3f>();
    //     if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active)))
    //         return { bs, 0.f };
    //     Float cos_theta_i = Frame3f::cos_theta(si.wi);
    //     auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));
    //     Float t_i = dr::maximum(1.0 - r_i, 0.0);
    //     Mask selected_r;
    //     Mask outside;
    //     outside = cos_theta_i > 0.f;
    //     // std::cout<<cos_theta_t<<std::endl;
    //     selected_r = (sample1 <= r_i) && active;
    //     bs.eta = dr::select(selected_r, Float(1.f), eta_it);
        
    //     float random = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
        
        
    //     if (random < f*dielectric_vs_scatter){
    //         bs.pdf = dr::select(selected_r, r_i, t_i);
    //         bs.wo = dr::select(selected_r,
    //                     reflect(si.wi),
    //                     refract(si.wi, cos_theta_t, eta_ti));
    //         Spectrum value_t = dr::sqr(eta_ti);
    //         Spectrum value_r(1.f);
    //         Spectrum value = dr::select(selected_r, value_r, value_t);
    //         dr::masked(bs.sampled_type, selected_r) = +BSDFFlags::DeltaReflection;
    //         dr::masked(bs.sampled_type, 1-selected_r) = +BSDFFlags::DeltaTransmission;
    //         return { bs,value/dielectric_vs_scatter & active };
    //     }
        
    //     Float next_eta = dr::select((outside&selected_r)||(!outside&!selected_r), ext_ior, int_ior);
    //     Float now_eta = dr::select(outside, ext_ior, int_ior);

    //     Float scaling = 1; //area size, naive = 0 has already include this part.
    //     SurfaceInteraction3f si_temp_ = si;
    //     Mask discard;
    //     if (naive == 0){
    //         auto [pos_, pdf] = m_reflectance->sample_position(sample2, active);
    //         bs.pdf = Float(pdf);
    //         Point2f temp_pos = (pos_-0.5)*4/scale;
    //         Float x = (temp_pos[0]-now_eta*si.wi[0])/next_eta;
    //         Float y = (temp_pos[1]-now_eta*si.wi[1])/next_eta;
    //         Float x2y2 = x*x + y*y;
    //         si_temp_.uv = Point2f(pos_);
            
    //         discard = x2y2>1;
    //         Float sign = dr::select((outside&selected_r)||(!outside&!selected_r),1, -1);
    //         Float z = dr::select(discard, 0, sign*dr::sqrt(1-x2y2));
            
    //         Point3f wo(x,y,z);
    //         bs.wo = wo;
    //         bs.pdf = bs.pdf/(16/ dr::abs(z)/next_eta/next_eta);
    //     } else {
    //         Float sign = dr::select((outside&selected_r)||(!outside&!selected_r),1, -1);
    //         bs.wo = sign*warp::square_to_cosine_hemisphere(sample2);
    //         bs.pdf = warp::square_to_cosine_hemisphere_pdf(dr::abs(bs.wo+__FLT_EPSILON__));
    //         Point3f projection = bs.wo*next_eta+si.wi*now_eta;
    //         Point2f pos(projection[0],projection[1]);
    //         pos = (pos/4*scale+0.5);
    //         si_temp_.uv = Point2f(pos);
    //         scaling *= scale*scale;
    //     }

    //     Spectrum fr = scaling*m_reflectance->eval(si_temp_, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(-bs.wo)+__FLT_EPSILON__));
    //     fr /= (1-f); //normalize
    //     bs.pdf = bs.pdf*(1-f*dielectric_vs_scatter)/(1-f);
    //     Spectrum value = fr/(bs.pdf+__FLT_EPSILON__)*dr::abs(bs.wo.z());
    //     value = dr::select(discard, 0, value);
    //     bs.sampled_component = dr::select(selected_r, UInt32(0), UInt32(1));
    //     bs.sampled_type      = dr::select(selected_r, UInt32(+BSDFFlags::GlossyReflection),
    //                                                 UInt32(+BSDFFlags::GlossyTransmission));

    //     return { bs,value & active };}

    // dummy function
    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, 
                                             const SurfaceInteraction3f &si, 
                                             Float sample1, 
                                             const Point2f &sample2, 
                                             Mask active) const override {
                                                return sample_more(ctx, si, sample1, sample2, sample1, sample2, active);
                                             }
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
        
        float wavelength_base =350;
        auto scale_wave = si.wavelengths/wavelength_base;
        Float t_i = dr::maximum(1.0 - r_i, 0.0);

        Mask outside;
        outside = cos_theta_i > 0.f;
        
        Mask selected_r_for_scatter = sample1<=r_i;
        Float next_eta = dr::select((outside&selected_r_for_scatter)||(!outside&!selected_r_for_scatter), ext_ior, int_ior);
        Float now_eta = dr::select(outside, ext_ior, int_ior);
        Float next_eta2 = next_eta*next_eta;
        Float scaling = scale*scale; //area size
        
        SurfaceInteraction3f si_temp_ = si;
        Mask discard(0.f);
        Float pdf_scatter(0.f);
        Vector3f fake_si = dr::select(selected_r_for_scatter, si.wi, -refract(si.wi, cos_theta_t, eta_ti));
        Spectrum fr(0);
        
        // Point2f temp_pos(0.f);
        // Float selected_wave = si.wavelengths[0];
        // auto [pos_I, pdf_I] = I_a->sample_position(sample2, active);
        // temp_pos = (pos_I-0.5)*4/scale*selected_wave/wavelength_base;
        // si_temp_.uv = Point2f(pos_I);
        // for (int i = 0; i<4;i++){
        //     Point2f pos_(temp_pos[0],temp_pos[1]);
        //     pos_ = (pos_/4*scale/scale_wave[i]+0.5);
        //     si_temp_.uv = Point2f(pos_);
        //     fr[i] = I_a->eval(si_temp_, active)[0]/dr::square(si.wavelengths[i]/selected_wave);
        // }
        // Float x = (temp_pos[0]-next_eta*fake_si[0])/next_eta;
        // Float y = (temp_pos[1]-next_eta*fake_si[1])/next_eta;
        // Float x2y2 = x*x + y*y;
        // discard = x2y2>1;
        // Float sign = dr::select((outside&selected_r_for_scatter)||(!outside&!selected_r_for_scatter),1, -1);
        // Float z = dr::select(discard, 0, sign*dr::sqrt(1-x2y2));
        // Vector3f wo{x,y,z};
        // bs.wo = wo;
        // Float J1 =16 / (dr::abs(z)+__FLT_EPSILON__)/next_eta2/dr::square(selected_wave/wavelength_base);
        // pdf_scatter = pdf_I/J1;
        // scaling = 1;

        if (naive == 0){
            Point2f temp_pos(0.f);
            Float selected_wave = si.wavelengths[0];
        
            // dr::select(sample22[1]<0.25, si.wavelengths[0],si.wavelengths[1]);
            // selected_wave = dr::select(sample22[1]<0.5, selected_wave,si.wavelengths[2]);
            // selected_wave = dr::select(sample22[1]<0.75, selected_wave,si.wavelengths[3]);
            auto [pos_I, pdf_I] = I_a->sample_position(sample2, active);
            temp_pos = (pos_I-0.5)*4/scale*selected_wave/wavelength_base;
            si_temp_.uv = Point2f(pos_I);
            Float wave_min(1000);
            for (int i = 0; i<4;i++){
                wave_min = dr::select(wave_min<si.wavelengths[i], wave_min, si.wavelengths[i]);
            }

            Float dmin(0.0);
            Float dmax(dr::abs(Frame3f::sin_theta(si.wi))+1);
            
            // dmax = Float(2);
            dmax /= (wave_min/wavelength_base);
            Float cdf_min = m_distr_sk.eval_cdf_normalized(dmin/2, active);
            Float cdf_max = m_distr_sk.eval_cdf_normalized(dmax/2, active);
            auto [pos_d, pdf_d] = m_distr_sk.sample_pdf(cdf_min + sample2[0] * (cdf_max - cdf_min), active);

            Float beta = sample2[1]*2*dr::Pi<ScalarFloat>;
            Mask mis = sample1<0.5;
            Float pos_d_I_projection = dr::select(mis, pos_d, dr::sqrt(dr::square((pos_I-0.5)*4/scale)[0]+dr::square((pos_I-0.5)*4/scale)[1])/2);
            Float d = pos_d_I_projection*2*(selected_wave/wavelength_base);
            Point2f pos_d_I_point(d*dr::sin(beta),d*dr::cos(beta));
            pos_d_I_point = dr::select(mis, pos_d_I_point,temp_pos);
            

            pdf_d = m_distr_sk.eval_pdf_normalized(pos_d,active)/(cdf_max - cdf_min)*0.5;
            temp_pos = pos_d_I_point;
            Spectrum fr_d(0);
            for (int i = 0; i<4;i++){
                Point2f pos_(pos_d_I_point[0],pos_d_I_point[1]);
                pos_ = (pos_/4*scale/(si.wavelengths[i]/wavelength_base)+0.5);
                si_temp_.uv = Point2f(pos_);
                fr_d[i] = I_a->eval(si_temp_, active)[0]*m_distr_sk.eval_pdf(pos_d_I_projection, active)/dr::square(si.wavelengths[i]/wavelength_base);
            }
            Spectrum fr_I(0);
            for (int i = 0; i<4;i++){
                Point2f pos_(pos_d_I_point[0],pos_d_I_point[1]);
                pos_ = (pos_/4*scale/scale_wave[i]+0.5);
                si_temp_.uv = Point2f(pos_);
                fr_I[i] = I_a->eval(si_temp_, active)[0]/dr::square(si.wavelengths[i]/selected_wave);
            }
            fr = dr::select(mis, fr_d, fr_I);
            Float x = (temp_pos[0]-next_eta*fake_si[0])/next_eta;
            Float y = (temp_pos[1]-next_eta*fake_si[1])/next_eta;
            Float x2y2 = x*x + y*y;
            discard = x2y2>1;
            Float sign = dr::select((outside&selected_r_for_scatter)||(!outside&!selected_r_for_scatter),1, -1);
            Float z = dr::select(discard, 0, sign*dr::sqrt(1-x2y2));
            Vector3f wo{x,y,z};
            bs.wo = wo;
            scaling = 1;
            Float J1 =16 / (dr::abs(z)+__FLT_EPSILON__)/next_eta2/(selected_wave/wavelength_base);
            Float J2 = 4 * d / (dr::abs(z)+__FLT_EPSILON__)*(selected_wave/wavelength_base);
            pdf_scatter = (pdf_d/J2+pdf_I/J1)/2;
        } else {
            // Float sign = dr::select((outside&selected_r_for_scatter)||(!outside&!selected_r_for_scatter),1, -1);
            // bs.wo = sign*warp::square_to_cosine_hemisphere(sample2);
            // pdf_scatter = warp::square_to_cosine_hemisphere_pdf((dr::abs(bs.wo)+__FLT_EPSILON__));
            // Point3f projection = bs.wo*next_eta+fake_si*next_eta;
            // Point2f pos(projection[0],projection[1]);
            // Float projection_d = dr::sqrt(dr::square(projection[0])+dr::square(projection[1]));
            // si_temp_.uv = Point2f(pos);
            Point3f reverse {1,1,-1};
            bs.wo = warp::square_to_cosine_hemisphere(sample2)*reverse;
            
            pdf_scatter = warp::square_to_cosine_hemisphere_pdf((dr::abs(bs.wo)+__FLT_EPSILON__));
            Point3f projection = bs.wo*next_eta+si.wi*now_eta;
            Point2f pos(projection[0],projection[1]);
            Float projection_d = dr::sqrt(dr::square(projection[0])+dr::square(projection[1]));
            for (int i = 0; i<4;i++){
                Point2f pos_(projection[0],projection[1]);
                pos_ = (pos_/4*scale/scale_wave[i]+0.5);
                si_temp_.uv = Point2f(pos_);
                fr[i] = I_a->eval(si_temp_, active)[0]*m_distr_sk.eval_pdf(projection_d*scale/scale_wave[i]/2, active)/dr::square(scale_wave[i]);
            }
            // for (int i = 0; i<4;i++){
            //     Point2f pos_ = (pos/4*scale/scale_wave[i]+0.5);
            //     si_temp_.uv = Point2f(pos_);
            //     fr[i] = I_a->eval(si_temp_, active)[0]*m_distr_sk.eval_pdf(projection_d*scale/scale_wave[i]/2, active)/dr::square(scale_wave[i])/1.5;
            // } 
        }
        
        dr::masked(bs.sampled_type, selected_r_for_scatter) = +BSDFFlags::GlossyReflection;
        dr::masked(bs.sampled_type, 1-selected_r_for_scatter) = +BSDFFlags::GlossyTransmission;
        bs.sampled_component = dr::select(selected_r_for_scatter, UInt32(0), UInt32(1));
        bs.eta = dr::select(selected_r_for_scatter, Float(1.f), eta_it);

        active &=dr::select(discard, 0,1);
        // Spectrum fr(0);
        // std::cout<<fr[0]<<std::endl;
        //  Spectrum fr(wave1,wave2,wave3,wave4);

        fr *= next_eta2*scaling* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(bs.wo)+__FLT_EPSILON__));
        
     
        bs.pdf = dr::select(selected_r_for_scatter,  pdf_scatter*r_i, pdf_scatter*t_i);
        Spectrum value_scatter = dr::select(selected_r_for_scatter, fr*r_i, fr*t_i);
        Spectrum value = value_scatter/(bs.pdf+0.0001) *  dr::abs((Frame3f::cos_theta(bs.wo)+__FLT_EPSILON__));
        value = dr::select(discard, 0, value);
        bs.pdf = dr::select(discard, 0, bs.pdf);
        value = 0;
        return { bs, value / (1 - f) & (active && bs.pdf > 0.f) };
    }

    Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo, Mask active) const override {
        if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active)))
            return 0.f;
        
        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));

        Mask reflect = cos_theta_i*cos_theta_o>0;
        Float pdf_portion = dr::select(reflect, Float(r_i), 1-Float(r_i));

        Mask outside;
        Mask selected_r;
        selected_r = cos_theta_i*cos_theta_o>0&active;
        outside = cos_theta_i > 0.f;
        Float next_eta = dr::select((outside&selected_r)||(!outside&!selected_r), ext_ior, int_ior);
        Float now_eta = dr::select(outside, ext_ior, int_ior);

        Point3f projection = wo*next_eta+si.wi*now_eta;
        Point2f pos(projection[0],projection[1]);
        pos = (pos/4*scale+0.5);
        
        Mask discard = pos.x()<0||pos.y()<0||pos.x()>1||pos.y()>1;
        Float scaling = scale*scale;
        SurfaceInteraction3f si_temp = si;
        si_temp.uv = pos;
        Float pdf = warp::square_to_cosine_hemisphere_pdf(dr::abs(wo+__FLT_EPSILON__));
        if(naive == 0){
            Float J = (16/(dr::abs(wo.z())+__FLT_EPSILON__)/next_eta/next_eta)*scaling;
            pdf = m_reflectance->pdf_position(pos,active)/J;
            // /(16/ dr::abs(wo.z()+__FLT_EPSILON__)/next_eta/next_eta)*scaling;
        } 

        pdf = pdf*pdf_portion+0.0001;
        Spectrum fr(0);
        if (sk_I_seperate)
            fr = I_a->eval(si_temp, active)*sk->eval(si_temp, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
        else
            fr = m_reflectance->eval(si_temp, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
        Spectrum value = fr*scaling*pdf_portion * dr::abs(cos_theta_o)/(1-f);
        // std::cout<<"eval"<<std::endl;
    
        return { value};
    }

    Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo, Mask active) const override {
        // C++ 实现PDF逻辑
        if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active)))
            return 0.f;
        
        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));

        Mask reflect = cos_theta_i*cos_theta_o>0;
        Float pdf_portion = dr::select(reflect, Float(r_i), 1-Float(r_i));

        Mask outside;
        Mask selected_r;
        selected_r = cos_theta_i*cos_theta_o>0&active;
        outside = cos_theta_i > 0.f;
        Float next_eta = dr::select((outside&selected_r)||(!outside&!selected_r), ext_ior, int_ior);
        Float now_eta = dr::select(outside, ext_ior, int_ior);

        Point3f projection = wo*next_eta+si.wi*now_eta;
        Point2f pos(projection[0],projection[1]);
        pos = (pos/4*scale+0.5);
        
        Mask discard = pos.x()<0||pos.y()<0||pos.x()>1||pos.y()>1;
        Float scaling = scale*scale;
        SurfaceInteraction3f si_temp = si;
        si_temp.uv = pos;
        Float pdf = warp::square_to_cosine_hemisphere_pdf(dr::abs(wo+__FLT_EPSILON__));
        if(naive == 0){
            Float J = (16/ dr::abs(wo.z()+__FLT_EPSILON__)/next_eta/next_eta)*scaling*dr::Pi<ScalarFloat>;
            pdf = m_reflectance->pdf_position(pos,active)/J;
            // /(16/ dr::abs(wo.z()+__FLT_EPSILON__)/next_eta/next_eta);//*scaling;
        } 

        pdf = pdf*pdf_portion;
        Spectrum fr(0);
        if (sk_I_seperate)
            fr = I_a->eval(si_temp, active)*sk->eval(si_temp, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
        else
            fr = m_reflectance->eval(si_temp, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
        
        
        // Spectrum fr = m_reflectance->eval(si_temp, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
        Spectrum value = fr*scaling*pdf_portion * dr::abs(cos_theta_o)/(1-f);
        // std::cout<<"pdf"<<std::endl;
        return {pdf};
    }
    std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
                                        const SurfaceInteraction3f &si,
                                        const Vector3f &wo,
                                        Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);
        if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active)))
            return { 0.f, 0.f };
        Float wavelength_base (350);
        auto scale_wave = si.wavelengths/wavelength_base;
        Float cos_theta_i = Frame3f::cos_theta(si.wi),
              cos_theta_o = Frame3f::cos_theta(wo);

        auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));
        
        Mask reflect = cos_theta_i*cos_theta_o>0;
        Float pdf_portion = dr::select(reflect, Float(r_i), 1-Float(r_i));
        Vector3f fake_si = dr::select(reflect, si.wi, -refract(si.wi, cos_theta_t, eta_ti));

        // std::cout<<pdf_portion<<std::endl;
        
        Mask outside;
        Mask selected_r;
        selected_r = cos_theta_i*cos_theta_o>0&active;
        outside = cos_theta_i > 0.f;
        
        Float next_eta = dr::select((outside&selected_r)||(!outside&!selected_r), ext_ior, int_ior);
        Float next_eta2 = next_eta*next_eta;


        Point3f projection = wo*next_eta+fake_si*next_eta;
        Point2f pos(projection[0],projection[1]);
        pos = (pos/4*scale+0.5);
        
        active = !(pos.x()<0||pos.y()<0||pos.x()>1||pos.y()>1);
        Float scale2 = scale*scale;
        
        SurfaceInteraction3f si_temp = si;
        si_temp.uv = pos;
        Float pdf = warp::square_to_cosine_hemisphere_pdf(dr::abs(wo+__FLT_EPSILON__));
        // if(naive == 0){
        //     Float J = (16/ (dr::abs(wo.z())+__FLT_EPSILON__)/next_eta2);//*scale;
        //     if(sample_sk_I_seperate)
        //         pdf = Float(sk->pdf_position(pos,active)+I_a->pdf_position(pos,active))/2/J;
        //     else
        //         pdf = m_reflectance->pdf_position(pos,active)/J;
        // } 
        
        Spectrum fr(0);
        
        if (naive == 0){
            Spectrum fr_d(0.f), fr_I(0.f);
            Float selected_wave = si.wavelengths[0];
            Float wave_min(1000);
            for (int i = 0; i<4;i++){
                wave_min = dr::select(wave_min<si.wavelengths[i], wave_min, si.wavelengths[i]);
            }
            Float dmin(0.0);
            Float dmax(dr::abs(Frame3f::sin_theta(si.wi))+1);
            dmax /= (wave_min/wavelength_base);
            Float cdf_min = m_distr_sk.eval_cdf_normalized(dmin/2, active);
            Float cdf_max = m_distr_sk.eval_cdf_normalized(dmax/2, active);
            
            Float projection_d = dr::sqrt(dr::square(projection[0])+dr::square(projection[1]));
            for (int i = 0; i < 4; ++i) {
                Point2f pos_(projection[0],projection[1]);
                pos_ = (pos_/4*scale/si.wavelengths[i]*wavelength_base+0.5);
                si_temp.uv = Point2f(pos_);
                fr[i] = I_a->eval(si_temp, active)[0]*m_distr_sk.eval_pdf(projection_d*scale/si.wavelengths[i]*selected_wave/2, active)/dr::square(scale_wave[i]);
            }
            
            Float pdf_d = m_distr_sk.eval_pdf_normalized(projection_d*scale/selected_wave*wavelength_base/2, active) / (cdf_max - cdf_min) * 0.5f;
            Float pdf_I = I_a->pdf_position((pos+0.5)/4*scale/selected_wave*wavelength_base, active); // <- 你需要有这个函数
            
            Float J1 = 16.f / (dr::abs(wo.z()) + __FLT_EPSILON__) / next_eta2 / (selected_wave / wavelength_base);
            Float J2 = 4.f * projection_d / (dr::abs(wo.z()) + __FLT_EPSILON__) * (selected_wave / wavelength_base);
            pdf = (pdf_d / J2 + pdf_I / J1) * 0.5f;
        }else{
            Float projection_d = dr::sqrt(dr::square(projection[0])+dr::square(projection[1]));
            for (int i = 0; i<4;i++){
                Point2f pos_(projection[0],projection[1]);
                pos_ = (pos_/4*scale/scale_wave[i]+0.5);
                si_temp.uv = Point2f(pos_);
                fr[i] = I_a->eval(si_temp, active)[0]*m_distr_sk.eval_pdf(projection_d*scale/scale_wave[i]/2, active)/dr::square(scale_wave[i]);
            }
            // fr *= next_eta2*scale2* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__))*1000;
        }
        fr *= next_eta2*scale2* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
        pdf = dr::select(reflect,  pdf*r_i, pdf*(1-r_i));
        Spectrum value = dr::select(selected_r, fr*r_i, fr*(1-r_i))*  dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
        return { value/(1-f)&active, dr::select(active, pdf, 0.f)};
     }

    
    std::string to_string() const override {
        std::ostringstream oss;
        oss << "particle2d_IS_texture[" << std::endl
            << "  eta = " << m_eta << "," << std::endl
            << "  radius = " << radius << "," << std::endl
            << "  fraction = " << fraction << std::endl
            << "]";
        return oss.str();
    }
    MI_DECLARE_CLASS()
private:
    float m_eta, radius, fraction, scale, f;
    ContinuousDistribution<Float> m_distr_sk;
    mutable float counter;
    mutable float totalcounter;
    ScalarFloat ext_ior, int_ior;
    // auto rng = mi::PCG32();
    int naive = 0;
    int sk_I_seperate = 0;
    int sample_sk_I_seperate = 0;
    float area;
    ref<Texture> m_reflectance;
    ref<Texture> sk;
    ref<Texture> I_a;
};

MI_IMPLEMENT_CLASS_VARIANT(particle2d_IS_texture, BSDF)
MI_EXPORT_PLUGIN(particle2d_IS_texture, "Particle 2D importance sample")
NAMESPACE_END(mitsuba)
// #include <mitsuba/core/properties.h>
// #include <mitsuba/core/warp.h>
// #include <mitsuba/render/bsdf.h>
// #include <mitsuba/render/fresnel.h>
// #include <mitsuba/render/ior.h>
// #include <mitsuba/render/texture.h>
// #include <mitsuba/core/spectrum.h>
// #include <iostream>
// #include <cxxabi.h>
// #include <fstream>
// #include <sstream>
// #include <vector>
// #include <string>
// #include <filesystem>
// #include <mitsuba/core/distr_2d.h>
// namespace fs = std::filesystem;
// NAMESPACE_BEGIN(mitsuba)

// template <typename Float, typename Spectrum>
// class particle2d_IS_texture final : public BSDF<Float, Spectrum> {
// public:
//     MI_IMPORT_BASE(BSDF, m_flags, m_components)
//     MI_IMPORT_TYPES(Texture)

//     particle2d_IS_texture(const Properties &props) : Base(props) {
//         m_reflectance = props.texture<Texture>("reflectance", .5f);
//         sk = props.texture<Texture>("sk", 1.f);
//         I_a = props.texture<Texture>("Ia", 1.f);
// // bk7

//         int_ior = lookup_ior(props, "int_ior", "bk7");

//         // Specifies the external index of refraction at the interface
//         ext_ior = lookup_ior(props, "ext_ior", "air");

//         m_components.push_back(BSDFFlags::DeltaReflection|BSDFFlags::GlossyReflection | BSDFFlags::FrontSide |
//                                BSDFFlags::BackSide);
//         m_components.push_back(BSDFFlags::DeltaTransmission|BSDFFlags::GlossyTransmission | BSDFFlags::FrontSide |
//                                BSDFFlags::BackSide);

//         m_flags = m_components[0] | m_components[1];
//         m_eta = int_ior / ext_ior;
//         radius = props.get<float>("radius",5);
//         fraction = props.get<float>("f", 0.5);
//         scale = props.get<float>("scale",1);
//         naive = props.get<int>("naive",0);
//         sk_I_seperate = props.get<int>("sk_I_seperate",0);
//         sample_sk_I_seperate = props.get<int>("sample_sk_I_seperate",0);
//         // dielectric_vs_scatter = props.get<float>("vs",1);
//         f = props.get<float>("f",0.5);
//         std::cout<<int_ior<<std::endl;
//         std::cout<<ext_ior<<std::endl;
//         counter=1;
//         totalcounter=1;
       
//     }
//     void traverse(TraversalCallback *callback) override {
//         callback->put_object("reflectance", m_reflectance.get(), +ParamFlags::Differentiable);
//         callback->put_object("Ia", I_a.get(), +ParamFlags::Differentiable);
//         callback->put_object("sk", sk.get(), +ParamFlags::Differentiable);
//     }
//     // std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, 
//     //                                          const SurfaceInteraction3f &si, 
//     //                                          Float sample1, 
//     //                                          const Point2f &sample2, 
//     //                                          Mask active) const override {
//     //     MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);
//     //     BSDFSample3f bs = dr::zeros<BSDFSample3f>();
//     //     if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active)))
//     //         return { bs, 0.f };
//     //     Float cos_theta_i = Frame3f::cos_theta(si.wi);
//     //     auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));
//     //     Float t_i = dr::maximum(1.0 - r_i, 0.0);
//     //     Mask selected_r;
//     //     Mask outside;
//     //     outside = cos_theta_i > 0.f;
//     //     // std::cout<<cos_theta_t<<std::endl;
//     //     selected_r = (sample1 <= r_i) && active;
//     //     bs.eta = dr::select(selected_r, Float(1.f), eta_it);
        
//     //     float random = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
        
        
//     //     if (random < f*dielectric_vs_scatter){
//     //         bs.pdf = dr::select(selected_r, r_i, t_i);
//     //         bs.wo = dr::select(selected_r,
//     //                     reflect(si.wi),
//     //                     refract(si.wi, cos_theta_t, eta_ti));
//     //         Spectrum value_t = dr::sqr(eta_ti);
//     //         Spectrum value_r(1.f);
//     //         Spectrum value = dr::select(selected_r, value_r, value_t);
//     //         dr::masked(bs.sampled_type, selected_r) = +BSDFFlags::DeltaReflection;
//     //         dr::masked(bs.sampled_type, 1-selected_r) = +BSDFFlags::DeltaTransmission;
//     //         return { bs,value/dielectric_vs_scatter & active };
//     //     }
        
//     //     Float next_eta = dr::select((outside&selected_r)||(!outside&!selected_r), ext_ior, int_ior);
//     //     Float now_eta = dr::select(outside, ext_ior, int_ior);

//     //     Float scaling = 1; //area size, naive = 0 has already include this part.
//     //     SurfaceInteraction3f si_temp_ = si;
//     //     Mask discard;
//     //     if (naive == 0){
//     //         auto [pos_, pdf] = m_reflectance->sample_position(sample2, active);
//     //         bs.pdf = Float(pdf);
//     //         Point2f temp_pos = (pos_-0.5)*4/scale;
//     //         Float x = (temp_pos[0]-now_eta*si.wi[0])/next_eta;
//     //         Float y = (temp_pos[1]-now_eta*si.wi[1])/next_eta;
//     //         Float x2y2 = x*x + y*y;
//     //         si_temp_.uv = Point2f(pos_);
            
//     //         discard = x2y2>1;
//     //         Float sign = dr::select((outside&selected_r)||(!outside&!selected_r),1, -1);
//     //         Float z = dr::select(discard, 0, sign*dr::sqrt(1-x2y2));
            
//     //         Point3f wo(x,y,z);
//     //         bs.wo = wo;
//     //         bs.pdf = bs.pdf/(16/ dr::abs(z)/next_eta/next_eta);
//     //     } else {
//     //         Float sign = dr::select((outside&selected_r)||(!outside&!selected_r),1, -1);
//     //         bs.wo = sign*warp::square_to_cosine_hemisphere(sample2);
//     //         bs.pdf = warp::square_to_cosine_hemisphere_pdf(dr::abs(bs.wo+__FLT_EPSILON__));
//     //         Point3f projection = bs.wo*next_eta+si.wi*now_eta;
//     //         Point2f pos(projection[0],projection[1]);
//     //         pos = (pos/4*scale+0.5);
//     //         si_temp_.uv = Point2f(pos);
//     //         scaling *= scale*scale;
//     //     }

//     //     Spectrum fr = scaling*m_reflectance->eval(si_temp_, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(-bs.wo)+__FLT_EPSILON__));
//     //     fr /= (1-f); //normalize
//     //     bs.pdf = bs.pdf*(1-f*dielectric_vs_scatter)/(1-f);
//     //     Spectrum value = fr/(bs.pdf+__FLT_EPSILON__)*dr::abs(bs.wo.z());
//     //     value = dr::select(discard, 0, value);
//     //     bs.sampled_component = dr::select(selected_r, UInt32(0), UInt32(1));
//     //     bs.sampled_type      = dr::select(selected_r, UInt32(+BSDFFlags::GlossyReflection),
//     //                                                 UInt32(+BSDFFlags::GlossyTransmission));

//     //     return { bs,value & active };}

//     // dummy function
//     std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, 
//                                              const SurfaceInteraction3f &si, 
//                                              Float sample1, 
//                                              const Point2f &sample2, 
//                                              Mask active) const override {
//                                                 return sample_more(ctx, si, sample1, sample2, sample1, sample2, active);
//                                              }
//     std::pair<BSDFSample3f, Spectrum> sample_more(const BSDFContext &ctx, 
//                                              const SurfaceInteraction3f &si, 
//                                              Float sample1, 
//                                              const Point2f &sample2, 
//                                              Float sample11, 
//                                              const Point2f &sample22, 
//                                              Mask active) const override {
//         MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);
//         BSDFSample3f bs = dr::zeros<BSDFSample3f>();
//         if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active)))
//             return { bs, 0.f };
//         Float cos_theta_i = Frame3f::cos_theta(si.wi);
//         auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));
        
        
//         Float t_i = dr::maximum(1.0 - r_i, 0.0);

//         Mask outside;
//         outside = cos_theta_i > 0.f;
        
//         Mask selected_r_for_scatter = sample1<=r_i;
//         Float next_eta = dr::select((outside&selected_r_for_scatter)||(!outside&!selected_r_for_scatter), ext_ior, int_ior);
//         Float now_eta = dr::select(outside, ext_ior, int_ior);
//         Float next_eta2 = next_eta*next_eta;
//         Float scaling = scale*scale; //area size
        
//         SurfaceInteraction3f si_temp_ = si;
//         Mask discard(0.f);
//         Float pdf_scatter(0.f);
//         Vector3f fake_si = dr::select(selected_r_for_scatter, si.wi, -refract(si.wi, cos_theta_t, eta_ti));

//         // Mask cosine_sampling_combine = dr::select()
//         if (naive == 0){
//             Point2f temp_pos(0.f);
//             if (sample_sk_I_seperate == 1) {
//                 auto [pos_, pdf]  = dr::select(sample11<0.5, sk->sample_position(sample2, active),I_a->sample_position(sample2, active));
//                 pdf_scatter = Float(sk->pdf_position(pos_,active)+I_a->pdf_position(pos_,active))/2;
//                 temp_pos = (pos_-0.5)*4/scale;
//                 si_temp_.uv = Point2f(pos_);
                
//             } else if(sample_sk_I_seperate == 0){
//                 auto [pos_, pdf] = m_reflectance->sample_position(sample2, active);
//                 pdf_scatter = Float(pdf);
//                 temp_pos = (pos_-0.5)*4/scale;
//                 si_temp_.uv = Point2f(pos_);
//             } else if(sample_sk_I_seperate == 2){
//                 auto [pos_, pdf]  = dr::select(sample11<1/3, sk->sample_position(sample2, active),I_a->sample_position(sample2, active));
//                 pdf_scatter = Float(pdf);
//                 temp_pos = (pos_-0.5)*4/scale;
//                 si_temp_.uv = Point2f(pos_);
//             }
//             Float x = (temp_pos[0]-next_eta*fake_si[0])/next_eta;
//             Float y = (temp_pos[1]-next_eta*fake_si[1])/next_eta;
//             Float x2y2 = x*x + y*y;
            
//             discard = x2y2>1;
//             Float sign = dr::select((outside&selected_r_for_scatter)||(!outside&!selected_r_for_scatter),1, -1);
//             Float z = dr::select(discard, 0, sign*dr::sqrt(1-x2y2));
//             Vector3f wo{x,y,z};
//             bs.wo = wo;
//             Float J =16/ (dr::abs(z)+__FLT_EPSILON__)/next_eta2;
//             pdf_scatter = pdf_scatter/J;
//             scaling = 1;

//         } else {
//             Float sign = dr::select((outside&selected_r_for_scatter)||(!outside&!selected_r_for_scatter),1, -1);
//             bs.wo = sign*warp::square_to_cosine_hemisphere(sample2);
//             pdf_scatter = warp::square_to_cosine_hemisphere_pdf((dr::abs(bs.wo)+__FLT_EPSILON__));
//             Point3f projection = bs.wo*next_eta+fake_si*next_eta;
//             Point2f pos(projection[0],projection[1]);
//             pos = (pos/4*scale+0.5);
//             si_temp_.uv = Point2f(pos);
//         }
        
//         dr::masked(bs.sampled_type, selected_r_for_scatter) = +BSDFFlags::GlossyReflection;
//         dr::masked(bs.sampled_type, 1-selected_r_for_scatter) = +BSDFFlags::GlossyTransmission;
//         bs.sampled_component = dr::select(selected_r_for_scatter, UInt32(0), UInt32(1));
//         bs.eta = dr::select(selected_r_for_scatter, Float(1.f), eta_it);

//         active &=dr::select(discard, 0,1);
//         Spectrum fr(0);
//         if (sk_I_seperate)
//             fr = next_eta2*scaling*I_a->eval(si_temp_, active)*sk->eval(si_temp_, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(bs.wo)+__FLT_EPSILON__));
//         else
//             fr = next_eta2*scaling*m_reflectance->eval(si_temp_, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(bs.wo)+__FLT_EPSILON__));

//         bs.pdf = dr::select(selected_r_for_scatter,  pdf_scatter*r_i, pdf_scatter*t_i);
//         Spectrum value_scatter = dr::select(selected_r_for_scatter, fr*r_i, fr*t_i);
//         Spectrum value = value_scatter/(bs.pdf+0.0001) *  dr::abs((Frame3f::cos_theta(bs.wo)+__FLT_EPSILON__));
//         value = dr::select(discard, 0, value);
//         bs.pdf = dr::select(discard, 0, bs.pdf);
//         // Spectrum value_(1);
//         // value_[1] = 0;
//         // value_[2] = 0;
//         // value_[0] = 0;
//         // bs.pdf = 1;
//         return { bs,value/(1-f) & (active && bs.pdf > 0.f) }; 
//     }
// // value/(1-f)
//     Spectrum eval(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo, Mask active) const override {
//         if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active)))
//             return 0.f;
        
//         Float cos_theta_i = Frame3f::cos_theta(si.wi),
//               cos_theta_o = Frame3f::cos_theta(wo);

//         auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));

//         Mask reflect = cos_theta_i*cos_theta_o>0;
//         Float pdf_portion = dr::select(reflect, Float(r_i), 1-Float(r_i));

//         Mask outside;
//         Mask selected_r;
//         selected_r = cos_theta_i*cos_theta_o>0&active;
//         outside = cos_theta_i > 0.f;
//         Float next_eta = dr::select((outside&selected_r)||(!outside&!selected_r), ext_ior, int_ior);
//         Float now_eta = dr::select(outside, ext_ior, int_ior);

//         Point3f projection = wo*next_eta+si.wi*now_eta;
//         Point2f pos(projection[0],projection[1]);
//         pos = (pos/4*scale+0.5);
        
//         Mask discard = pos.x()<0||pos.y()<0||pos.x()>1||pos.y()>1;
//         Float scaling = scale*scale;
//         SurfaceInteraction3f si_temp = si;
//         si_temp.uv = pos;
//         Float pdf = warp::square_to_cosine_hemisphere_pdf(dr::abs(wo+__FLT_EPSILON__));
//         if(naive == 0){
//             Float J = (16/(dr::abs(wo.z())+__FLT_EPSILON__)/next_eta/next_eta)*scaling;
//             pdf = m_reflectance->pdf_position(pos,active)/J;
//             // /(16/ dr::abs(wo.z()+__FLT_EPSILON__)/next_eta/next_eta)*scaling;
//         } 

//         pdf = pdf*pdf_portion+0.0001;
//         Spectrum fr(0);
//         if (sk_I_seperate)
//             fr = I_a->eval(si_temp, active)*sk->eval(si_temp, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
//         else
//             fr = m_reflectance->eval(si_temp, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
//         Spectrum value = fr*scaling*pdf_portion * dr::abs(cos_theta_o)/(1-f);
//         // std::cout<<"eval"<<std::endl;

//         return { value};
//     }

//     Float pdf(const BSDFContext &ctx, const SurfaceInteraction3f &si, const Vector3f &wo, Mask active) const override {
//         // C++ 实现PDF逻辑
//         if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active)))
//             return 0.f;
        
//         Float cos_theta_i = Frame3f::cos_theta(si.wi),
//               cos_theta_o = Frame3f::cos_theta(wo);

//         auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));

//         Mask reflect = cos_theta_i*cos_theta_o>0;
//         Float pdf_portion = dr::select(reflect, Float(r_i), 1-Float(r_i));

//         Mask outside;
//         Mask selected_r;
//         selected_r = cos_theta_i*cos_theta_o>0&active;
//         outside = cos_theta_i > 0.f;
//         Float next_eta = dr::select((outside&selected_r)||(!outside&!selected_r), ext_ior, int_ior);
//         Float now_eta = dr::select(outside, ext_ior, int_ior);

//         Point3f projection = wo*next_eta+si.wi*now_eta;
//         Point2f pos(projection[0],projection[1]);
//         pos = (pos/4*scale+0.5);
        
//         Mask discard = pos.x()<0||pos.y()<0||pos.x()>1||pos.y()>1;
//         Float scaling = scale*scale;
//         SurfaceInteraction3f si_temp = si;
//         si_temp.uv = pos;
//         Float pdf = warp::square_to_cosine_hemisphere_pdf(dr::abs(wo+__FLT_EPSILON__));
//         if(naive == 0){
//             Float J = (16/ dr::abs(wo.z()+__FLT_EPSILON__)/next_eta/next_eta)*scaling*M_PI;
//             pdf = m_reflectance->pdf_position(pos,active)/J;
//             // /(16/ dr::abs(wo.z()+__FLT_EPSILON__)/next_eta/next_eta);//*scaling;
//         } 

//         pdf = pdf*pdf_portion;
//         Spectrum fr(0);
//         if (sk_I_seperate)
//             fr = I_a->eval(si_temp, active)*sk->eval(si_temp, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
//         else
//             fr = m_reflectance->eval(si_temp, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
        
        
//         // Spectrum fr = m_reflectance->eval(si_temp, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
//         Spectrum value = fr*scaling*pdf_portion * dr::abs(cos_theta_o)/(1-f);
//         std::cout<<"pdf"<<std::endl;
//         return {pdf};
//     }
//     std::pair<Spectrum, Float> eval_pdf(const BSDFContext &ctx,
//                                         const SurfaceInteraction3f &si,
//                                         const Vector3f &wo,
//                                         Mask active) const override {
//         MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);
//         if (unlikely(!ctx.is_enabled(BSDFFlags::GlossyReflection) || dr::none_or<false>(active)))
//             return { 0.f, 0.f };
        
//         Float cos_theta_i = Frame3f::cos_theta(si.wi),
//               cos_theta_o = Frame3f::cos_theta(wo);

//         auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));
        
//         Mask reflect = cos_theta_i*cos_theta_o>0;
//         Float pdf_portion = dr::select(reflect, Float(r_i), 1-Float(r_i));
//         Vector3f fake_si = dr::select(reflect, si.wi, -refract(si.wi, cos_theta_t, eta_ti));

//         // std::cout<<pdf_portion<<std::endl;
        
//         Mask outside;
//         Mask selected_r;
//         selected_r = cos_theta_i*cos_theta_o>0&active;
//         outside = cos_theta_i > 0.f;
        
//         Float next_eta = dr::select((outside&selected_r)||(!outside&!selected_r), ext_ior, int_ior);
//         Float next_eta2 = next_eta*next_eta;


//         Point3f projection = wo*next_eta+fake_si*next_eta;
//         Point2f pos(projection[0],projection[1]);
//         pos = (pos/4*scale+0.5);
        
//         active = !(pos.x()<0||pos.y()<0||pos.x()>1||pos.y()>1);
//         Float scale2 = scale*scale;
        
//         SurfaceInteraction3f si_temp = si;
//         si_temp.uv = pos;
//         Float pdf = warp::square_to_cosine_hemisphere_pdf(dr::abs(wo+__FLT_EPSILON__));
//         if(naive == 0){
//             Float J = (16/ (dr::abs(wo.z())+__FLT_EPSILON__)/next_eta2);//*scale;
//             if(sample_sk_I_seperate)
//                 pdf = Float(sk->pdf_position(pos,active)+I_a->pdf_position(pos,active))/2/J;
//             else
//                 pdf = m_reflectance->pdf_position(pos,active)/J;
//             // next_eta2 = 1;
//         } 
//         Spectrum fr(0);
//         //  std::cout<<fr*wavelength_base<<std::endl;
//         dr::print(si.wavelengths);
//         if (sk_I_seperate)
//             fr = next_eta2*scale2*I_a->eval(si_temp, active)*sk->eval(si_temp, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
//         else
//             fr = next_eta2*scale2*m_reflectance->eval(si_temp, active)* dr::abs(cos_theta_i) / dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
//         pdf = dr::select(reflect,  pdf*r_i, pdf*(1-r_i));
//         Spectrum value = dr::select(selected_r, fr*r_i, fr*(1-r_i))*  dr::abs((Frame3f::cos_theta(wo)+__FLT_EPSILON__));
    
//         // bs.pdf = 1;
//         return { value/(1-f)&active, dr::select(active, pdf, 0.f)};
//      }

    
//     std::string to_string() const override {
//         std::ostringstream oss;
//         oss << "particle2d_IS_texture[" << std::endl
//             << "  eta = " << m_eta << "," << std::endl
//             << "  radius = " << radius << "," << std::endl
//             << "  fraction = " << fraction << std::endl
//             << "]";
//         return oss.str();
//     }
//     MI_DECLARE_CLASS()
// private:
//     float m_eta, radius, fraction, scale, f;
//     mutable float counter;
//     mutable float totalcounter;
//     ScalarFloat ext_ior, int_ior;
//     // auto rng = mi::PCG32();
//     int naive = 0;
//     int sk_I_seperate = 0;
//     int sample_sk_I_seperate = 0;
//     float area;
//     ref<Texture> m_reflectance;
//     ref<Texture> sk;
//     ref<Texture> I_a;
// };

// MI_IMPLEMENT_CLASS_VARIANT(particle2d_IS_texture, BSDF)
// MI_EXPORT_PLUGIN(particle2d_IS_texture, "Particle 2D importance sample")
// NAMESPACE_END(mitsuba)
