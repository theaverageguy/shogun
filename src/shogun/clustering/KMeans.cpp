/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Written (W) 1999-2008 Gunnar Raetsch
 * Written (W) 2007-2009 Soeren Sonnenburg
 * Copyright (C) 1999-2009 Fraunhofer Institute FIRST and Max-Planck-Society
 */

#include <shogun/clustering/KMeansLloydImpl.h>
#include "shogun/clustering/KMeansMiniBatchImpl.h"
#include <shogun/clustering/KMeans.h>
#include <shogun/distance/Distance.h>
#include <shogun/distance/EuclideanDistance.h>
#include <shogun/labels/Labels.h>
#include <shogun/features/DenseFeatures.h>
#include <shogun/mathematics/Math.h>
#include <shogun/base/Parallel.h>

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

using namespace shogun;

CKMeans::CKMeans()
: CDistanceMachine()
{
	init();
}

CKMeans::CKMeans(int32_t k_, CDistance* d, EKMeansMethod f)
:CDistanceMachine()
{
	init();
	k=k_;
	set_distance(d);
	train_method=f;
}

CKMeans::CKMeans(int32_t k_, CDistance* d, bool use_kmpp, EKMeansMethod f)
: CDistanceMachine()
{
	init();
	k=k_;
	set_distance(d);
	use_kmeanspp=use_kmpp;
	train_method=f;
}

CKMeans::CKMeans(int32_t k_i, CDistance* d_i, SGMatrix<float64_t> centers_i, EKMeansMethod f)
: CDistanceMachine()
{
	init();
	k = k_i;
	set_distance(d_i);
	set_initial_centers(centers_i);
	train_method=f;
}

CKMeans::~CKMeans()
{
}

void CKMeans::set_initial_centers(SGMatrix<float64_t> centers)
{
 	CDenseFeatures<float64_t>* lhs=((CDenseFeatures<float64_t>*) distance->get_lhs());
	dimensions=lhs->get_num_features();
	REQUIRE(centers.num_cols == k,
			"Expected %d initial cluster centers, got %d", k, centers.num_cols);
	REQUIRE(centers.num_rows == dimensions,
			"Expected %d dimensionional cluster centers, got %d", dimensions, centers.num_rows);
	mus_initial = centers;
	SG_UNREF(lhs);
}

void CKMeans::set_random_centers()
{
	mus.zero();
	CDenseFeatures<float64_t>* lhs=
		CDenseFeatures<float64_t>::obtain_from_generic(distance->get_lhs());
	int32_t lhs_size=lhs->get_num_vectors();
	
	SGVector<int32_t> temp=SGVector<int32_t>(lhs_size);
	SGVector<int32_t>::range_fill_vector(temp, lhs_size, 0);
	CMath::permute(temp);

	for (int32_t i=0; i<k; i++)
	{
		const int32_t cluster_center_i=temp[i];
		SGVector<float64_t> vec=lhs->get_feature_vector(cluster_center_i);

		for (int32_t j=0; j<dimensions; j++)
			mus(j,i)=vec[j];

		lhs->free_feature_vector(vec, cluster_center_i);
	}

	SG_UNREF(lhs);

}

void CKMeans::compute_cluster_variances()
{
	/* compute the ,,variances'' of the clusters */
	for (int32_t i=0; i<k; i++)
	{
		float64_t rmin1=0;
		float64_t rmin2=0;

		bool first_round=true;

		for (int32_t j=0; j<k; j++)
		{
			if (j!=i)
			{
				int32_t l;
				float64_t dist = 0;

				for (l=0; l<dimensions; l++)
				{
					dist+=CMath::sq(
							mus.matrix[i*dimensions+l]
									-mus.matrix[j*dimensions+l]);
				}

				if (first_round)
				{
					rmin1=dist;
					rmin2=dist;
					first_round=false;
				}
				else
				{
					if ((dist<rmin2) && (dist>=rmin1))
						rmin2=dist;

					if (dist<rmin1)
					{
						rmin2=rmin1;
						rmin1=dist;
					}
				}
			}
		}

		R.vector[i]=(0.7*CMath::sqrt(rmin1)+0.3*CMath::sqrt(rmin2));
	}
}

bool CKMeans::train_machine(CFeatures* data)
{
	REQUIRE(distance, "Distance is not provided")
	REQUIRE(distance->get_feature_type()==F_DREAL, "Distance's features type (%d) should be of type REAL (%d)")
	
	if (data)
		distance->init(data, data);

	CDenseFeatures<float64_t>* lhs=
		CDenseFeatures<float64_t>::obtain_from_generic(distance->get_lhs());

	REQUIRE(lhs, "Lhs features of distance not provided");
	int32_t lhs_size=lhs->get_num_vectors();
	dimensions=lhs->get_num_features();
	const int32_t centers_size=dimensions*k;

	REQUIRE(lhs_size>0, "Lhs features should not be empty");
	REQUIRE(dimensions>0, "Lhs features should have more than zero dimensions");

	/* if kmeans++ to be used */
	if (use_kmeanspp)
		mus_initial=kmeanspp();

	R=SGVector<float64_t>(k);

	mus=SGMatrix<float64_t>(dimensions, k);
	/* cluster_centers=zeros(dimensions, k) ; */
	memset(mus.matrix, 0, sizeof(float64_t)*centers_size);


	if (mus_initial.matrix)
		mus = mus_initial;
	else
		set_random_centers();

	if (train_method==KMM_MINI_BATCH)
	{
		CKMeansMiniBatchImpl::minibatch_KMeans(k, distance, batch_size, minib_iter, mus);
	}
	else
	{
		CKMeansLloydImpl::Lloyd_KMeans(k, distance, max_iter, mus, fixed_centers);
	}

	compute_cluster_variances();
	SG_UNREF(lhs);
	return true;
}

bool CKMeans::load(FILE* srcfile)
{
	SG_SET_LOCALE_C;
	SG_RESET_LOCALE;
	return false;
}

bool CKMeans::save(FILE* dstfile)
{
	SG_SET_LOCALE_C;
	SG_RESET_LOCALE;
	return false;
}

void CKMeans::set_use_kmeanspp(bool kmpp)
{
	use_kmeanspp=kmpp;
}

bool CKMeans::get_use_kmeanspp() const
{
	return use_kmeanspp;
}

void CKMeans::set_k(int32_t p_k)
{
	REQUIRE(p_k>0, "number of clusters should be > 0");
	this->k=p_k;
}

int32_t CKMeans::get_k()
{
	return k;
}

void CKMeans::set_max_iter(int32_t iter)
{
	REQUIRE(iter>0, "number of clusters should be > 0");
	max_iter=iter;
}

float64_t CKMeans::get_max_iter()
{
	return max_iter;
}

void CKMeans::set_train_method(EKMeansMethod f)
{
	train_method=f;
}

EKMeansMethod CKMeans::get_train_method() const
{
	return train_method;
}

void CKMeans::set_mbKMeans_batch_size(int32_t b)
{
	REQUIRE(b>0, "Parameter bach size should be > 0");
	batch_size=b;
}

int32_t CKMeans::get_mbKMeans_batch_size() const
{
	return batch_size;
}

void CKMeans::set_mbKMeans_iter(int32_t i)
{
	REQUIRE(i>0, "Parameter number of iterations should be > 0");
	minib_iter=i;
}

int32_t CKMeans::get_mbKMeans_iter() const
{
	return minib_iter;
}

void CKMeans::set_mbKMeans_params(int32_t b, int32_t t)
{
	REQUIRE(b>0, "Parameter bach size should be > 0");
	REQUIRE(t>0, "Parameter number of iterations should be > 0");
	batch_size=b;
	minib_iter=t;
}

SGVector<float64_t> CKMeans::get_radiuses()
{
	return R;
}

SGMatrix<float64_t> CKMeans::get_cluster_centers()
{
	if (!R.vector)
		return SGMatrix<float64_t>();

	CDenseFeatures<float64_t>* lhs=
		(CDenseFeatures<float64_t>*)distance->get_lhs();
	SGMatrix<float64_t> centers=lhs->get_feature_matrix();
	SG_UNREF(lhs);
	return centers;
}

int32_t CKMeans::get_dimensions()
{
	return dimensions;
}

void CKMeans::set_fixed_centers(bool fixed)
{
	fixed_centers=fixed;
}

bool CKMeans::get_fixed_centers()
{
	return fixed_centers;
}

void CKMeans::store_model_features()
{
	/* set lhs of underlying distance to cluster centers */
	CDenseFeatures<float64_t>* cluster_centers=new CDenseFeatures<float64_t>(
			mus);

	/* store cluster centers in lhs of distance variable */
	CFeatures* rhs=distance->get_rhs();
	distance->init(cluster_centers, rhs);
	SG_UNREF(rhs);
}

SGMatrix<float64_t> CKMeans::kmeanspp()
{
	int32_t num=distance->get_num_vec_lhs();
	SGVector<float64_t> dists=SGVector<float64_t>(num);
	SGVector<int32_t> mu_index=SGVector<int32_t>(k);

	/* 1st center */
	int32_t mu_1=CMath::random((int32_t) 0,num-1);
	mu_index[0]=mu_1;

	/* choose a center - do k-1 times */
	int32_t count=0;
	while (++count<k)
	{
		float64_t sum=0.0;
		/* for each data point find distance to nearest already chosen center */
		for (int32_t point_idx=0;point_idx<num;point_idx++)
		{
			dists[point_idx]=distance->distance(mu_index[0],point_idx);
			int32_t cent_id=1;

			while (cent_id<count)
			{
				float64_t dist_temp=distance->distance(mu_index[cent_id],point_idx);
				if (dists[point_idx]>dist_temp)
					dists[point_idx]=dist_temp;
				cent_id++;
			}

			dists[point_idx]*=dists[point_idx];
			sum+=dists[point_idx];
		}

		/*random choosing - points weighted by square of distance from nearset center*/
		int32_t mu_next=0;
		float64_t chosen=CMath::random(0.0,sum);
		while ((chosen-=dists[mu_next])>0)
			mu_next++;

		mu_index[count]=mu_next;
	}

	CDenseFeatures<float64_t>* lhs=(CDenseFeatures<float64_t>*)distance->get_lhs();
	int32_t dim=lhs->get_num_features();
	SGMatrix<float64_t> mat=SGMatrix<float64_t>(dim,k);
	for (int32_t c_m=0;c_m<k;c_m++)
	{
		SGVector<float64_t> feature=lhs->get_feature_vector(c_m);
		for (int32_t r_m=0;r_m<dim;r_m++)
			mat(r_m,c_m)=feature[r_m];
	}
	SG_UNREF(lhs);
	return mat;
}

void CKMeans::init()
{
	max_iter=10000;
	k=3;
	dimensions=0;
	fixed_centers=false;
	use_kmeanspp=false;
	train_method=KMM_LLOYD;
	batch_size=-1;
	minib_iter=-1;
	SG_ADD(&max_iter, "max_iter", "Maximum number of iterations", MS_AVAILABLE);
	SG_ADD(&k, "k", "k, the number of clusters", MS_AVAILABLE);
	SG_ADD(&dimensions, "dimensions", "Dimensions of data", MS_NOT_AVAILABLE);
	SG_ADD(&R, "R", "Cluster radiuses", MS_NOT_AVAILABLE);
}

