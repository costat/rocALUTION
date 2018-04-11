#include "../../utils/def.hpp"
#include "ruge_stueben_amg.hpp"

#include "../../base/local_matrix.hpp"
#include "../../base/local_vector.hpp"

#include "../preconditioners/preconditioner_multicolored_gs.hpp"

#include "../../utils/log.hpp"
#include "../../utils/math_functions.hpp"

namespace paralution {

template <class OperatorType, class VectorType, typename ValueType>
RugeStuebenAMG<OperatorType, VectorType, ValueType>::RugeStuebenAMG() {

  LOG_DEBUG(this, "RugeStuebenAMG::RugeStuebenAMG()",
            "default constructor");

  // parameter for strong couplings in smoothed aggregation
  this->eps_   = ValueType(0.25f);

}

template <class OperatorType, class VectorType, typename ValueType>
RugeStuebenAMG<OperatorType, VectorType, ValueType>::~RugeStuebenAMG() {

  LOG_DEBUG(this, "RugeStuebenAMG::RugeStuebenAMG()",
            "destructor");

  this->Clear();

}

template <class OperatorType, class VectorType, typename ValueType>
void RugeStuebenAMG<OperatorType, VectorType, ValueType>::Print(void) const {

  LOG_INFO("AMG solver");
  LOG_INFO("AMG number of levels " << this->levels_);
  LOG_INFO("AMG using Ruge-Stüben coarsening");
  LOG_INFO("AMG coarsest operator size = " << this->op_level_[this->levels_-2]->get_nrow());
  LOG_INFO("AMG coarsest level nnz = " <<this->op_level_[this->levels_-2]->get_nnz());
  LOG_INFO("AMG with smoother:");
  this->smoother_level_[0]->Print();
  
}

template <class OperatorType, class VectorType, typename ValueType>
void RugeStuebenAMG<OperatorType, VectorType, ValueType>::PrintStart_(void) const {

  assert(this->levels_ > 0);

  LOG_INFO("AMG solver starts");
  LOG_INFO("AMG number of levels " << this->levels_);
  LOG_INFO("AMG using Ruge-Stüben coarsening");
  LOG_INFO("AMG coarsest operator size = " << this->op_level_[this->levels_-2]->get_nrow());
  LOG_INFO("AMG coarsest level nnz = " <<this->op_level_[this->levels_-2]->get_nnz());
  LOG_INFO("AMG with smoother:");
  this->smoother_level_[0]->Print();

}

template <class OperatorType, class VectorType, typename ValueType>
void RugeStuebenAMG<OperatorType, VectorType, ValueType>::PrintEnd_(void) const {

    LOG_INFO("AMG ends");

}

template <class OperatorType, class VectorType, typename ValueType>
void RugeStuebenAMG<OperatorType, VectorType, ValueType>::SetCouplingStrength(const ValueType eps) {

  LOG_DEBUG(this, "RugeStuebenAMG::SetCouplingStrength()",
            eps);

  this->eps_ = eps;

}

template <class OperatorType, class VectorType, typename ValueType>
void RugeStuebenAMG<OperatorType, VectorType, ValueType>::BuildSmoothers(void) {

  LOG_DEBUG(this, "RugeStuebenAMG::BuildSmoothers()",
            " #*# begin");

  // Smoother for each level
  FixedPoint<OperatorType, VectorType, ValueType > **sm = NULL;
  sm = new FixedPoint<OperatorType, VectorType, ValueType >* [this->levels_-1];

  this->smoother_level_ = new IterativeLinearSolver<OperatorType, VectorType, ValueType>*[this->levels_-1];
  this->sm_default_ = new Solver<OperatorType, VectorType, ValueType>*[this->levels_-1];

  MultiColoredGS<OperatorType, VectorType, ValueType > **gs = NULL;
  gs = new MultiColoredGS<OperatorType, VectorType, ValueType >* [this->levels_-1];

  for (int i=0; i<this->levels_-1; ++i) {
    sm[i] = new FixedPoint<OperatorType, VectorType, ValueType >;
    gs[i] = new MultiColoredGS<OperatorType, VectorType, ValueType >;

    gs[i]->SetPrecondMatrixFormat(this->sm_format_);
    sm[i]->SetRelaxation(ValueType(1.3f));
    sm[i]->SetPreconditioner(*gs[i]);
    sm[i]->Verbose(0);
    this->smoother_level_[i] = sm[i];
    this->sm_default_[i] = gs[i];
  }

  delete[] gs;
  delete[] sm;

}

template <class OperatorType, class VectorType, typename ValueType>
void RugeStuebenAMG<OperatorType, VectorType, ValueType>::ReBuildNumeric(void) {

  LOG_DEBUG(this, "RugeStuebenAMG::ReBuildNumeric()",
            " #*# begin");

  assert(this->levels_ > 1);
  assert(this->build_);
  assert(this->op_ != NULL);

  this->op_level_[0]->Clear();
  this->op_level_[0]->ConvertToCSR();

  if (this->op_->get_format() != CSR) {
    OperatorType op_csr;
    op_csr.CloneFrom(*this->op_);
    op_csr.ConvertToCSR();

    // Create coarse operator
    OperatorType tmp;
    tmp.CloneBackend(*this->op_);
    this->op_level_[0]->CloneBackend(*this->op_);

    OperatorType *cast_res = dynamic_cast<OperatorType*>(this->restrict_op_level_[0]);
    OperatorType *cast_pro = dynamic_cast<OperatorType*>(this->prolong_op_level_[0]);
    assert(cast_res != NULL);
    assert(cast_pro != NULL);

    tmp.MatrixMult(*cast_res, op_csr);
    this->op_level_[0]->MatrixMult(tmp, *cast_pro);

  } else {

    // Create coarse operator
    OperatorType tmp;
    tmp.CloneBackend(*this->op_);
    this->op_level_[0]->CloneBackend(*this->op_);

    OperatorType *cast_res = dynamic_cast<OperatorType*>(this->restrict_op_level_[0]);
    OperatorType *cast_pro = dynamic_cast<OperatorType*>(this->prolong_op_level_[0]);
    assert(cast_res != NULL);
    assert(cast_pro != NULL);

    tmp.MatrixMult(*cast_res, *this->op_);
    this->op_level_[0]->MatrixMult(tmp, *cast_pro);

  }

  for (int i=1; i<this->levels_-1; ++i) {

    this->op_level_[i]->Clear();
    this->op_level_[i]->ConvertToCSR();

    // Create coarse operator
    OperatorType tmp;
    tmp.CloneBackend(*this->op_);
    this->op_level_[i]->CloneBackend(*this->op_);

    OperatorType *cast_res = dynamic_cast<OperatorType*>(this->restrict_op_level_[i]);
    OperatorType *cast_pro = dynamic_cast<OperatorType*>(this->prolong_op_level_[i]);
    assert(cast_res != NULL);
    assert(cast_pro != NULL);

    if (i == this->levels_ - this->host_level_ - 1)
      this->op_level_[i-1]->MoveToHost();

    tmp.MatrixMult(*cast_res, *this->op_level_[i-1]);
    this->op_level_[i]->MatrixMult(tmp, *cast_pro);

    if (i == this->levels_ - this->host_level_ - 1)
      this->op_level_[i-1]->CloneBackend(*this->restrict_op_level_[i-1]);

  }

  for (int i=0; i<this->levels_-1; ++i) {

    if (i > 0)
      this->smoother_level_[i]->ResetOperator(*this->op_level_[i-1]);
    else
      this->smoother_level_[i]->ResetOperator(*this->op_);


    this->smoother_level_[i]->ReBuildNumeric();
    this->smoother_level_[i]->Verbose(0);
  }

  this->solver_coarse_->ResetOperator(*this->op_level_[this->levels_-2]);
  this->solver_coarse_->ReBuildNumeric();
  this->solver_coarse_->Verbose(0);

  // Convert operator to op_format
  if (this->op_format_ != CSR)
    for (int i=0; i<this->levels_-1;++i)
      this->op_level_[i]->ConvertTo(this->op_format_);

}

template <class OperatorType, class VectorType, typename ValueType>
void RugeStuebenAMG<OperatorType, VectorType, ValueType>::Aggregate(const OperatorType &op,
                                                                    Operator<ValueType> *pro,
                                                                    Operator<ValueType> *res,
                                                                    OperatorType *coarse) {

  LOG_DEBUG(this, "RugeStuebenAMG::Aggregate()",
            this->build_);

  assert(pro    != NULL);
  assert(res    != NULL);
  assert(coarse != NULL);

  OperatorType *cast_res = dynamic_cast<OperatorType*>(res);
  OperatorType *cast_pro = dynamic_cast<OperatorType*>(pro);

  assert(cast_res != NULL);
  assert(cast_pro != NULL);

  // Create prolongation and restriction operators
  op.RugeStueben(this->eps_, cast_pro, cast_res);

  // Create coarse operator
  OperatorType tmp;
  tmp.CloneBackend(op);
  coarse->CloneBackend(op);

  tmp.MatrixMult(*cast_res, op);
  coarse->MatrixMult(tmp, *cast_pro);

}


template class RugeStuebenAMG< LocalMatrix<double>, LocalVector<double>, double >;
template class RugeStuebenAMG< LocalMatrix<float>,  LocalVector<float>, float >;
#ifdef SUPPORT_COMPLEX
template class RugeStuebenAMG< LocalMatrix<std::complex<double> >, LocalVector<std::complex<double> >, std::complex<double> >;
template class RugeStuebenAMG< LocalMatrix<std::complex<float> >,  LocalVector<std::complex<float> >,  std::complex<float> >;
#endif

}