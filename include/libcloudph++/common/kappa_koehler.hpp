#pragma once

#include <libcloudph++/common/units.hpp>
#include <libcloudph++/common/macros.hpp>
#include <libcloudph++/common/kelvin_term.hpp>

#include <boost/math/tools/toms748_solve.hpp>

namespace libcloudphxx
{
  namespace common
  {
    namespace kappa_koehler
    {
      /// @brief equilibrium wet radius to the third power, with the Kelvin term discarded, for a given:
      /// @arg dry radius to the third power
      /// @arg the solubility parameter kappa
      /// @arg ratio of abmient vapour density/pressure to saturation vapour density/pressure for pure water
      /// 
      /// the formula stems from applying the kappa-Koehler relation 
      /// (eq. 6 in @copydetails Petters_and_Kreidenweis_2007) to a stationarity
      /// condition for a vapour diffusion equation, which translates to
      /// zero differece of vapour density: rho_ambient - rho_surface = 0
      /// 
      /// since rho_surface = rho_surface_pure_water * a(r_w, r_d, kappa)
      /// one can derive r_w as a function of r_d, kappa and the ratio
      /// of abmient and surface vapour densities
      /// 
      /// for the kappa-Koehler parameterisation rw3 is linear with rd3
      template <typename real_t>
      quantity<si::volume, real_t> rw3_eq_nokelvin(
	quantity<si::volume, real_t> rd3, 
	quantity<si::dimensionless, real_t> kappa,
	quantity<si::dimensionless, real_t> vap_ratio
      )   
      {   
	return rd3 * (1 - vap_ratio * (1 - kappa)) / (1 - vap_ratio);
      }   

      /// @brief activity of water in solution (eqs. 1,6) in @copydetails Petters_and_Kreidenweis_2007
      template <typename real_t>
      quantity<si::dimensionless, real_t> a_w(
	quantity<si::volume, real_t> rw3,
	quantity<si::volume, real_t> rd3,
	quantity<si::dimensionless, real_t> kappa
      )
      {
	return (rw3 - rd3) / (rw3 - rd3 * (real_t(1) - kappa));
      }

      // @brief equilibrium wet radius to the third power for a given:
      /// @arg dry radius to the third power
      /// @arg the solubility parameter kappa
      /// @arg ratio of abmient vapour density/pressure to saturation vapour density/pressure for pure water
      template <typename real_t>
      quantity<si::volume, real_t> rw3_eq(
	quantity<si::volume, real_t> rd3, 
	quantity<si::dimensionless, real_t> kappa,
	quantity<si::dimensionless, real_t> vap_ratio,
	quantity<si::temperature, real_t> T
      )   
      {   
	// local functor to be passed to the minimisation func
	struct f 
	{   
	  const quantity<si::dimensionless, real_t> vap_ratio;
	  const quantity<si::volume, real_t> rd3;
	  const quantity<si::dimensionless, real_t> kappa;
	  const quantity<si::temperature, real_t> T;

          // ctor
          f(
	    const quantity<si::dimensionless, real_t> &vap_ratio,
	    const quantity<si::volume, real_t> &rd3,
	    const quantity<si::dimensionless, real_t> &kappa,
	    const quantity<si::temperature, real_t> &T
          ) : vap_ratio(vap_ratio), rd3(rd3), kappa(kappa), T(T) {}
      
	  real_t operator()(real_t rw3)
	  {
	    return vap_ratio 
	      - a_w(rw3 * si::cubic_metres, rd3, kappa) 
	      * kelvin::klvntrm(pow(rw3, 1./3) * si::metres, T); 
	  }
	};  

	boost::uintmax_t iters = 20; 
	std::pair<real_t, real_t> range = boost::math::tools::toms748_solve(
	  f(vap_ratio, rd3, kappa, T), // the above-defined functor
	  real_t(rd3 / si::cubic_metres), // min
	  real_t(rw3_eq_nokelvin(rd3, kappa, vap_ratio) / si::cubic_metres), // max
	  boost::math::tools::eps_tolerance<real_t>(sizeof(real_t) * 8 / 2),
	  iters // the highest attainable precision with the algorithm according to Boost docs
	); // TODO: ignore error?
	return (range.first + range.second) / 2 * si::cubic_metres;
      }

    };
  };
};