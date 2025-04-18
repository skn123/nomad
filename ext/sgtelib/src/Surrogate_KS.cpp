/*-------------------------------------------------------------------------------------*/
/*  sgtelib - A surrogate model library for derivative-free optimization               */
/*  Version 2.0.3                                                                      */
/*                                                                                     */
/*  Copyright (C) 2012-2017  Sebastien Le Digabel - Ecole Polytechnique, Montreal      */ 
/*                           Bastien Talgorn - McGill University, Montreal             */
/*                                                                                     */
/*  Author: Bastien Talgorn                                                            */
/*  email: bastientalgorn@fastmail.com                                                 */
/*                                                                                     */
/*  This program is free software: you can redistribute it and/or modify it under the  */
/*  terms of the GNU Lesser General Public License as published by the Free Software   */
/*  Foundation, either version 3 of the License, or (at your option) any later         */
/*  version.                                                                           */
/*                                                                                     */
/*  This program is distributed in the hope that it will be useful, but WITHOUT ANY    */
/*  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A    */
/*  PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.   */
/*                                                                                     */
/*  You should have received a copy of the GNU Lesser General Public License along     */
/*  with this program. If not, see <http://www.gnu.org/licenses/>.                     */
/*                                                                                     */
/*  You can find information on sgtelib at https://github.com/bastientalgorn/sgtelib   */
/*-------------------------------------------------------------------------------------*/

#include "Surrogate_KS.hpp"

/*----------------------------*/
/*         constructor        */
/*----------------------------*/
SGTELIB::Surrogate_KS::Surrogate_KS ( SGTELIB::TrainingSet & trainingset,
                                      const SGTELIB::Surrogate_Parameters& param) :
  SGTELIB::Surrogate ( trainingset , param  ) {
  #ifdef SGTELIB_DEBUG
    std::cout << "constructor KS\n";
  #endif
}//


/*----------------------------*/
/*          destructor        */
/*----------------------------*/
SGTELIB::Surrogate_KS::~Surrogate_KS ( void ) {

}//


/*--------------------------------------*/
/*              display                 */
/*--------------------------------------*/
void SGTELIB::Surrogate_KS::display_private ( std::ostream & out ) const {
  out << "(No special members)\n";
}//


/*--------------------------------------*/
/*             build_private            */
/*--------------------------------------*/
bool SGTELIB::Surrogate_KS::build_private ( void ) {

  // Verify that the kernel is decreasing
  if ( !  kernel_is_decreasing(_param.get_kernel_type())){
    throw SGTELIB::Exception ( __FILE__ , __LINE__ ,
             "Surrogate_KS::build_private(): Kernel must be decreasing for KS model" );
  }
 
  _ready = true;
  return true;
}//

/*--------------------------------------*/
/*       predict_private (ZZs only)     */
/*--------------------------------------*/
void SGTELIB::Surrogate_KS::predict_private ( const SGTELIB::Matrix & XXs,
                                                    SGTELIB::Matrix * ZZs) {
  
  // i: index of a point in Xs
  // ixx: index of a point in XXs
  // j: index of an output (ie: a column of ZZs)
  int i,ixx,j;

  // pxx: nb of prediction points
  int pxx = XXs.get_nb_rows();

  // D : distance between points of XXs and other points of the trainingset
  SGTELIB::Matrix D = _trainingset.get_distances(XXs,get_matrix_Xs(),_param.get_distance_type());

  // Kernel shape coefficient
  //double ks = _param.get_kernel_coef() / _trainingset.get_Ds_mean();
  double ks = _param.get_kernel_coef() / _trainingset.get_Ds_mean();

  // Compute weights 
  SGTELIB::Matrix phi = kernel(_param.get_kernel_type(),ks,D);

  const SGTELIB::Matrix & Zs = get_matrix_Zs();

  SGTELIB::Matrix PhiZ = phi*Zs;
  SGTELIB::Matrix Div = phi.sum(2);
  Div.hadamard_inverse();
  *ZZs = SGTELIB::Matrix::diagA_product(Div,PhiZ);

  if (Div.has_inf()){
    // Loop on the points of XXs
    for (ixx=0 ; ixx<pxx ; ixx++){
      if ( isinf(Div.get(ixx,0)) ){
        // Need to use the limit behavior of kernels
        switch (_param.get_kernel_type()){
          case SGTELIB::KERNEL_D1:
          case SGTELIB::KERNEL_D4:
          case SGTELIB::KERNEL_D5:
            // imin is the index of the closest neighbor of xx in Xs
            i = D.get_min_index_row(ixx); 
            // Copy the output of this point
            ZZs->set_row( Zs.get_row(i) , ixx);
            break;
          case SGTELIB::KERNEL_D2:
          case SGTELIB::KERNEL_D3:
          case SGTELIB::KERNEL_D6:
            // Use the mean of the output over the trainingset
            for (j=0 ; j<_m ; j++){
              ZZs->set(ixx,j,_trainingset.get_Zs_mean(j));
            }
            break;
          default:
            throw SGTELIB::Exception ( __FILE__ , __LINE__ ,
                   "Surrogate_KS::predict_private: Unacceptable kernel type" );
        }
      }
    }
  }

}//


// Predict only objectives (used in Surrogate Ensemble Stat)
void SGTELIB::Surrogate_KS::predict_private_objective ( const std::vector<SGTELIB::Matrix *> & XXd,
                                                        SGTELIB::Matrix * ZZsurr_around            ) {
  check_ready(__FILE__,__FUNCTION__,__LINE__);

  const size_t pxx = XXd.size();
  const int nbd = XXd[0]->get_nb_rows();

  // ind: index of a point in Xs
  // d: index of a point in XXd[i]
  int ind,d;

  // Loop on all pxx points 
  for (int i=0 ; i<static_cast<int>(pxx) ; i++){
    // XXd[i] is of dimension nbd * _n

    // D : distance between points of XXd[i] and other points of the trainingset
    SGTELIB::Matrix D = _trainingset.get_distances(*(XXd[i]), get_matrix_Xs(), _param.get_distance_type());

    // Kernel shape coefficient
    double ks = _param.get_kernel_coef() / _trainingset.get_Ds_mean();

    // Compute weights 
    SGTELIB::Matrix phi = kernel(_param.get_kernel_type(), ks, D);

    // Get only objective values in Zs
    SGTELIB::Matrix Zs ("Zs", nbd, 1);
    Zs = get_matrix_Zs().get_col(0); // if no objective (for tests only)
    double Zs_mean = 0;
    for (int j=0 ; j<_m ; j++){
      if (_trainingset.get_bbo(j)==SGTELIB::BBO_OBJ){
        Zs = get_matrix_Zs().get_col(j);
        Zs_mean = _trainingset.get_Zs_mean(j);
        break;
      }
    }

    // Row i of ZZsurr_around is set to [f(x_i + d_1) , ... , f(x_i + d_nbd)]
    SGTELIB::Matrix PhiZ = phi*Zs;
    SGTELIB::Matrix Div = phi.sum(2);
    Div.hadamard_inverse();
    SGTELIB::Matrix temp;
    ZZsurr_around->set_row( ( SGTELIB::Matrix::diagA_product(Div, PhiZ) ).transpose() , i );

    if (Div.has_inf()){
      // Loop on the points of XXd[i]
      for (d=0 ; d<nbd ; d++){
        if ( isinf(Div.get(d,0)) ){
          // Need to use the limit behavior of kernels
          switch (_param.get_kernel_type()){
            case SGTELIB::KERNEL_D1:
            case SGTELIB::KERNEL_D4:
            case SGTELIB::KERNEL_D5:
              // imin is the index of the closest neighbor of xx in Xs
              ind = D.get_min_index_row(d); 
              // Copy the output of the point i
              ZZsurr_around->set( i,d, Zs.get(ind,0) );
              break;
            case SGTELIB::KERNEL_D2:
            case SGTELIB::KERNEL_D3:
            case SGTELIB::KERNEL_D6:
              // Use the mean of the output over the trainingset
              ZZsurr_around->set( i,d, Zs_mean);
              break;
            default:
              throw SGTELIB::Exception ( __FILE__ , __LINE__ ,
                    "Surrogate_KS::predict_private_objective: Unacceptable kernel type" );
          }
        }
      } // end for d
    }

  } // end for i

}



/*--------------------------------------*/
/*       get_matrix_Zvs                 */
/*--------------------------------------*/
const SGTELIB::Matrix * SGTELIB::Surrogate_KS::get_matrix_Zvs (void){

  check_ready(__FILE__,__FUNCTION__,__LINE__);

  // Check that it's NULL
  if (  !  _Zvs ){

    #ifdef SGTELIB_DEBUG
      std::cout << "Compute _Zvs\n";
    #endif

    // Init the matrix
    _Zvs = new SGTELIB::Matrix("Zvs",_p,_m);

    // i : index of point of the trainingset
    // j : index of an output of the trainingset
    // iv: index of the point which is excluded from the model construction, and where the
    //     cross-validation model is evaluated.
    int i,j,iv;

    // w : sum of the weights
    // wz : sum of (weights*output)
    // z : predicted output (= wz/z)
    double w,wz,z;

    // Construction of the phi matrix
    // phi(i,iv) = kernel( || x(i) - x(iv) || )
    //double ks = _param.get_kernel_coef() / _trainingset.get_Ds_mean();
    double ks = _param.get_kernel_coef() / _trainingset.get_Ds_mean();

    // D : distance between points of XXs and other points of the trainingset
    SGTELIB::Matrix D = _trainingset.get_distances(get_matrix_Xs(),get_matrix_Xs(),_param.get_distance_type());

    SGTELIB::Matrix phi;
    phi = kernel(_param.get_kernel_type(),ks,D);

    // Loop on the outputs
    for (j=0 ; j<_m ; j++){

      // Compute the LOO-CV prediction for output j
      for (iv=0 ; iv<_p ; iv++){
        w = 0;
        wz= 0;
        // Loop on the points of the trainingset
        for (i=0 ; i<_p ; i++){
          // exclude the point iv from the construction
          if (i!=iv){
            w += phi.get(i,iv);
            wz+= phi.get(i,iv)*_trainingset.get_Zs(i,j);
          }
        }
        
        // Compute z 
        if (w>EPSILON){
          // Normal method
          z = wz/w;
        }
        else{
          // Need to use the limit behavior of kernels
          switch (_param.get_kernel_type()){
            case SGTELIB::KERNEL_D1:
            case SGTELIB::KERNEL_D4:
            case SGTELIB::KERNEL_D5:
            {
              // Find the closest point to iv
              // Nb: We must have imin != iv
              double d;
              double dmin = SGTELIB::INF;
              int imin = 0;  // no need to init imin;
              // Loop on the points of the trainingset
              for (i=0 ; i<_p ; i++){

                d = D.get(i,iv);
                if ( (i!=iv) &&  (d<dmin) ){
                  dmin = d;
                  imin = i;
                }
              }
              // Copy the output of this point
              z = _trainingset.get_Zs(imin,j);
              break;
            }
            case SGTELIB::KERNEL_D2:
            case SGTELIB::KERNEL_D3:
            case SGTELIB::KERNEL_D6:
              // Use the mean of the output over the trainingset
              // Loop on the outputs
              z = _trainingset.get_Zs_mean(j);
              break;
            default:
              throw SGTELIB::Exception ( __FILE__ , __LINE__ ,
                     "Surrogate_KS::predict_private: Unacceptable kernel type" );
          }
        }// End of special case for computation of z

        // Affectation of the CV prediction
        _Zvs->set(iv,j,z);
      }

    }
    _Zvs->replace_nan(+INF);
    _Zvs->set_name("Zvs");
  }
  return _Zvs;

}//

/*--------------------------------------*/
/*       get_matrix_Zhs                 */
/*--------------------------------------*/
const SGTELIB::Matrix * SGTELIB::Surrogate_KS::get_matrix_Zhs (void){

  check_ready(__FILE__,__FUNCTION__,__LINE__);

  // Check that it's NULL
  if (  !  _Zhs ){

    #ifdef SGTELIB_DEBUG
      std::cout << "Compute _Zhs\n";
    #endif

    int ixx,j;

    // w : sum of the weights
    // wz : sum of (weights*output)
    double w;
    SGTELIB::Matrix wZ;

    // Init the matrix
    _Zhs = new SGTELIB::Matrix("Zhs",_p,_m);

    // Construction of the phi matrix
    double ks = _param.get_kernel_coef() / _trainingset.get_Ds_mean();
    SGTELIB::Matrix phi;
    SGTELIB::Matrix D = _trainingset.get_distances(get_matrix_Xs(),get_matrix_Xs(),_param.get_distance_type());
    phi = kernel(_param.get_kernel_type(),ks,D);
    SGTELIB::Matrix phi_ixx;
    const SGTELIB::Matrix & Zs = get_matrix_Zs();

    // Loop on the outputs
    for (j=0 ; j<_m ; j++){
      // Compute the prediction for output j
      for (ixx=0 ; ixx<_p ; ixx++){
        phi_ixx = phi.get_row(ixx);
        w = phi_ixx.sum();
        wZ = phi_ixx*Zs;
        _Zhs->set_row( wZ/w , ixx );
      }

    }

    _Zhs->replace_nan(+INF);
    _Zhs->set_name("Zhs"); 

  }
  return _Zhs;

}//



















