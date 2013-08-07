// vim:filetype=cpp
/** @file
  * @copyright University of Warsaw
  * @section LICENSE
  * GPLv3+ (see the COPYING file or http://www.gnu.org/licenses/)
  */

#include <thrust/reduce.h>
#include <thrust/iterator/transform_iterator.h>
#include <thrust/iterator/zip_iterator.h>

namespace libcloudphxx
{
  namespace lgrngn
  {

    namespace detail
    {
      template <typename real_t>
      struct range_filter
      {
        real_t min, max, scl;

        range_filter(real_t min, real_t max, real_t scl) : min(min), max(max), scl(scl) {}

        __device__
        real_t operator()(const real_t &y, const real_t &x)
        {
          return x > min && x < max ? scl*y : 0; // TODO: >=?
        }
      };
    }  

    template <typename real_t, int device>
    void particles<real_t, device>::impl::moms_rng(
      const real_t &min, const real_t &max, 
      const thrust_device::vector<real_t> &radii
    )
    {
      hskpng_sort(); 

      // transforming n -> n/dv if within range, else 0
      thrust_device::vector<real_t> &sorted_n_over_dv_within_range(tmp_device_real_part);

      real_t dv = opts.dx * opts.dy * opts.dz;

      thrust::permutation_iterator<
	typename thrust_device::vector<n_t>::iterator, 
	typename thrust_device::vector<thrust_size_t>::iterator
      > sorted_n(n.begin(), sorted_id.begin());

      thrust::permutation_iterator<
	typename thrust_device::vector<real_t>::const_iterator, 
	typename thrust_device::vector<thrust_size_t>::iterator
      > sorted_radii(radii.begin(), sorted_id.begin());

      thrust::transform(
	sorted_n, sorted_n + n_part,           // input - 1st arg
	sorted_radii,                          // input - 2nd arg
	sorted_n_over_dv_within_range.begin(), // output
	detail::range_filter<real_t>(min, max, 1/(opts.dx * opts.dy * opts.dz)) 
      );
    }

    namespace detail
    {
      template <typename real_t>
      struct moment_counter
      {
        real_t xp;

        moment_counter(real_t xp) : xp(xp) {}

        __device__
        real_t operator()(const thrust::tuple<real_t, real_t> &tpl)
        {
          const real_t n_over_dv = thrust::get<0>(tpl);
          const real_t x = thrust::get<1>(tpl);
          return n_over_dv * pow(x, xp); // TODO: check if xp=0 is optimised
        }
      };
    };

    template <typename real_t, int device>
    void particles<real_t, device>::impl::moms_calc(
      const thrust_device::vector<real_t> &radii,
      const real_t power
    )
    {
      // same as above
      thrust_device::vector<real_t> &sorted_n_over_dv_within_range(tmp_device_real_part);

      using pi_t = thrust::permutation_iterator<
        typename thrust_device::vector<real_t>::const_iterator,
        typename thrust_device::vector<thrust_size_t>::iterator
      >;
      using zip_it_t = thrust::zip_iterator<
	thrust::tuple<
	  typename thrust_device::vector<real_t>::iterator,
	  pi_t
	>
      >;

      thrust::pair<
        thrust_device::vector<thrust_size_t>::iterator,
        typename thrust_device::vector<real_t>::iterator
      > n = thrust::reduce_by_key(
        // input - keys
        sorted_ijk.begin(), sorted_ijk.end(),  
        // input - values
        thrust::transform_iterator<
          typename detail::moment_counter<real_t>,
          zip_it_t,
          real_t
        >(
	  zip_it_t(thrust::make_tuple(
            sorted_n_over_dv_within_range.begin(),
            pi_t(radii.begin(), sorted_id.begin())
          )),
          detail::moment_counter<real_t>(power)
        ),
        // output - keys
        count_ijk.begin(),
        // output - values
        count_mom.begin()
      );  

      count_n = n.first - count_ijk.begin();
      assert(count_n > 0 && count_n <= n_cell);
    }
  };  
};