// vim:filetype=cpp
/** @file
  * @copyright University of Warsaw
  * @section LICENSE
  * GPLv3+ (see the COPYING file or http://www.gnu.org/licenses/)
  */

#include <libcloudph++/common/theta_dry.hpp>

namespace libcloudphxx
{
  namespace lgrngn
  {
    namespace detail
    {
      template <typename real_t>
      struct dth : thrust::unary_function<const thrust::tuple<real_t, real_t, real_t>&, real_t>
      {
        BOOST_GPU_ENABLED
        real_t operator()(const thrust::tuple<real_t, real_t, real_t> &tpl) const
        {
          const quantity<si::dimensionless, real_t> 
            drv      = thrust::get<0>(tpl);
          const quantity<si::temperature, real_t> 
            T        = thrust::get<1>(tpl) * si::kelvins;
          const quantity<si::temperature, real_t> 
            th       = thrust::get<2>(tpl) * si::kelvins;

          return drv * common::theta_dry::d_th_d_rv(T, th) / si::kelvins;
        }
      };
    };

    // update th and rv according to change in liquid water volume
    // particles have to be sorted
    template <typename real_t, backend_t device>
    void particles_t<real_t, device>::impl::update_th_rv(
      thrust_device::vector<real_t> &drv // change in water vapor mixing ratio
    ) 
    {   
      if(!sorted) throw std::runtime_error("update_th_rv called on an unsorted set");

      // updating rv 
      assert(*thrust::min_element(rv.begin(), rv.end()) >= 0);
      thrust::transform(
        rv.begin(), rv.end(),  // input - 1st arg
        drv.begin(),           // input - 2nd arg
        rv.begin(),            // output
        thrust::plus<real_t>() 
      );
      assert(*thrust::min_element(rv.begin(), rv.end()) >= 0);

      // updating th
      {
        typedef thrust::zip_iterator<thrust::tuple<
          typename thrust_device::vector<real_t>::iterator,
          typename thrust_device::vector<real_t>::iterator,
          typename thrust_device::vector<real_t>::iterator
        > > zip_it_t;

        // apply dth
        thrust::transform(
          th.begin(), th.end(),          // input - 1st arg
          thrust::make_transform_iterator(
            zip_it_t(thrust::make_tuple(  
              drv.begin(),      // 
              T.begin(),        // dth = drv * d_th_d_rv(T, th)
              th.begin()        //
            )),
            detail::dth<real_t>()
          ),
          th.begin(),                 // output
          thrust::plus<real_t>()
        );
      }
    }

    // update particle-specific cell state
    // particles have to be sorted
    template <typename real_t, backend_t device>
    void particles_t<real_t, device>::impl::update_pstate(
      thrust_device::vector<real_t> &pstate, // cell characteristic
      thrust_device::vector<real_t> &pdstate // change in cell characteristic
    ) 
    {   
      if(!sorted) throw std::runtime_error("update_uh_rv called on an unsorted set");

printf("update pdstate\n");
debug::print(pdstate);

      // cell-wise change in state
      thrust_device::vector<real_t> &dstate(tmp_device_real_cell);
      // init dstate with 0s
      thrust::fill(dstate.begin(), dstate.end(), real_t(0));
      // calc sum of pdstate in each cell
      thrust::pair<
        thrust_device::vector<thrust_size_t>::iterator,
        typename thrust_device::vector<real_t>::iterator
      > n = thrust::reduce_by_key(
        sorted_ijk.begin(), sorted_ijk.end(),
        thrust::make_permutation_iterator(pdstate.begin(), sorted_ijk.begin()),
        count_ijk.begin(),
        count_mom.begin()
      );
      count_n = n.first - count_ijk.begin();

      // add this sum to dstate
      thrust::transform(
        count_mom.begin(), count_mom.begin() + count_n,                    // input - 1st arg
        thrust::make_permutation_iterator(dstate.begin(), count_ijk.begin()), // 2nd arg
        thrust::make_permutation_iterator(dstate.begin(), count_ijk.begin()), // output
        thrust::plus<real_t>()
      );
printf("update dstate\n");
debug::print(dstate);

      // add dstate to pstate
      thrust::transform(
        pstate.begin(), pstate.end(),
        thrust::make_permutation_iterator(dstate.begin(), ijk.begin()),
        pstate.begin(),
        thrust::plus<real_t>()
      );
    }

    // update cell state based on particle-specific cell state
    // particles have to be sorted
    template <typename real_t, backend_t device>
    void particles_t<real_t, device>::impl::update_state(
      thrust_device::vector<real_t> &state, // cell state
      thrust_device::vector<real_t> &pstate // particle-specific cell state (same for all particles in one cell)
    ) 
    {
     thrust::copy(
       pstate.begin(), pstate.end(),
       thrust::make_permutation_iterator(state.begin(), ijk.begin())
     );   
    }
  };  
};
