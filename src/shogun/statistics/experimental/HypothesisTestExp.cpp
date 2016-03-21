/*
 * Restructuring Shogun's statistical hypothesis testing framework.
 * Copyright (C) 2016  Soumyajit De
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the selfied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <shogun/lib/SGVector.h>
#include <shogun/mathematics/Math.h>
#include <shogun/statistics/experimental/HypothesisTestExp.h>
#include <shogun/statistics/experimental/internals/TestTypes.h>
#include <shogun/statistics/experimental/internals/DataManager.h>
#include <shogun/statistics/experimental/internals/KernelManager.h>

using namespace shogun;
using namespace internal;

struct CHypothesisTestExp::Self
{
	Self(index_t num_distributions, index_t num_kernels)
	: data_manager(num_distributions), kernel_manager(num_kernels)
	{
	}
	DataManager data_manager;
	KernelManager kernel_manager;
};

CHypothesisTestExp::CHypothesisTestExp(index_t num_distributions, index_t num_kernels) : CSGObject()
{
	self = std::unique_ptr<Self>(new CHypothesisTestExp::Self(num_distributions, num_kernels));
}

CHypothesisTestExp::~CHypothesisTestExp()
{
}

DataManager& CHypothesisTestExp::get_data_manager()
{
	return self->data_manager;
}

const DataManager& CHypothesisTestExp::get_data_manager() const
{
	return self->data_manager;
}

KernelManager& CHypothesisTestExp::get_kernel_manager()
{
	return self->kernel_manager;
}

const KernelManager& CHypothesisTestExp::get_kernel_manager() const
{
	return self->kernel_manager;
}

float64_t CHypothesisTestExp::compute_p_value(float64_t statistic)
{
	SGVector<float64_t> values = sample_null();

	std::sort(values.vector, values.vector + values.vlen);
	float64_t i = values.find_position_to_insert(statistic);

	return 1.0 - i / values.vlen;
}

float64_t CHypothesisTestExp::compute_threshold(float64_t alpha)
{
	float64_t result = 0;
	SGVector<float64_t> values = sample_null();

	std::sort(values.vector, values.vector + values.vlen);
	return values[index_t(CMath::floor(values.vlen * (1 - alpha)))];
}

float64_t CHypothesisTestExp::perform_test()
{
	return compute_p_value(compute_statistic());
}

bool CHypothesisTestExp::perform_test(float64_t alpha)
{
	float64_t p_value = perform_test();
	return p_value < alpha;
}

const char* CHypothesisTestExp::get_name() const
{
	return "HypothesisTestExp";
}
