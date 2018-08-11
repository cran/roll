#ifndef ROLL_H
#define ROLL_H

#define ARMA_DONT_PRINT_ERRORS

#include <RcppArmadillo.h>
#include <RcppParallel.h>
using namespace Rcpp;
using namespace RcppParallel;

// 'Worker' function for computing rolling products using an online algorithm
struct RollProdOnline : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_x;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::mat& arma_prod;         // destination (pass by reference)
  
  // initialize with source and destination
  RollProdOnline(const NumericMatrix x, const int n,
                 const int n_rows_x, const int n_cols_x,
                 const int width, const arma::vec arma_weights,
                 const int min_obs, const arma::uvec arma_any_na,
                 const bool na_restore, arma::mat& arma_prod)
    : x(x), n(n),
      n_rows_x(n_rows_x), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      min_obs(min_obs), arma_any_na(arma_any_na),
      na_restore(na_restore), arma_prod(arma_prod) { }
  
  // function call operator that iterates by column
  void operator()(std::size_t begin_col, std::size_t end_col) {
    for (std::size_t j = begin_col; j < end_col; j++) {
      
      int n_obs = 0;
      long double lambda = 0;
      long double n_new = 0;
      long double n_old = 0;
      long double n_exp = 0;
      long double w_new = 0;
      long double w_old = 0;
      long double x_new = 0;
      long double x_old = 0;
      long double prod_w = 1;
      long double prod_x = 1;
      
      if (width > 1) {
        lambda = arma_weights[n - 2] / arma_weights[n - 1]; // check already passed!
      } else {
        lambda = arma_weights[n - 1];
      }
      
      for (int i = 0; i < n_rows_x; i++) {
        
        // expanding window
        if (i < width) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j))) {
            n_obs += 1;
          }
          
          if ((arma_any_na[i] != 0) || std::isnan(x(i, j))) {
            
            n_new = n_obs;
            w_new = 1;
            x_new = 1;
            
          } else {
            
            n_new = n_obs - 1;
            w_new = arma_weights[n - 1];
            x_new = x(i, j);
            
          }
          
          if (n_new == 0) {
            n_exp = 1;
          } else if (n_new > n_old) {
            n_exp = n_exp * lambda;
          } else if (n_new < n_old) {
            n_exp = n_exp / lambda;
          }
          
          n_old = n_new;
          prod_w *= w_new * n_exp;
          prod_x *= x_new;
          
        }
        
        // rolling window
        if (i >= width) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
              ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)))) {
            
            n_obs += 1;
            
          } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j))) &&
            (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j))) {
            
            n_obs -= 1;
            
          }
          
          if ((arma_any_na[i] != 0) || std::isnan(x(i, j))) {
            
            n_new = n_obs;
            w_new = 1;
            x_new = 1;
            
          } else {
            
            n_new = n_obs - 1;
            w_new = arma_weights[n - 1];
            x_new = x(i, j);
            
          }
          
          if ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j))) {
            
            w_old = 1;
            x_old = 1;
            
          } else {
            
            w_old = arma_weights[0];
            x_old = x(i - width, j);
            
          }
          
          if (n_new == 0) {
            n_exp = 1;
          } else if (n_new > n_old) {
            n_exp = n_exp * lambda;
          } else if (n_new < n_old) {
            n_exp = n_exp / lambda;
          }
          
          n_old = n_new;
          prod_w *= w_new * n_exp / w_old;
          prod_x *= x_new / x_old;
          
        }
        
        // don't compute if missing value and 'na_restore' argument is true
        if ((!na_restore) || (na_restore && !std::isnan(x(i, j)))) {
          
          // compute the product
          if (n_obs >= min_obs) {
            arma_prod(i, j) = prod_w * prod_x;
          } else {
            arma_prod(i, j) = NA_REAL;
          }
          
        } else {
          
          // can be either NA or NaN
          arma_prod(i, j) = x(i, j);
          
        }
        
      }
      
    }
  }
  
};

// 'Worker' function for computing rolling sums using an online algorithm
struct RollSumOnline : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_x;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::mat& arma_sum;          // destination (pass by reference)
  
  // initialize with source and destination
  RollSumOnline(const NumericMatrix x, const int n,
                const int n_rows_x, const int n_cols_x,
                const int width, const arma::vec arma_weights,
                const int min_obs, const arma::uvec arma_any_na,
                const bool na_restore, arma::mat& arma_sum)
    : x(x), n(n),
      n_rows_x(n_rows_x), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      min_obs(min_obs), arma_any_na(arma_any_na),
      na_restore(na_restore), arma_sum(arma_sum) { }
  
  // function call operator that iterates by column
  void operator()(std::size_t begin_col, std::size_t end_col) {
    for (std::size_t j = begin_col; j < end_col; j++) {
      
      int n_obs = 0;
      long double lambda = 0;
      long double w_new = 0;
      long double w_old = 0;
      long double x_new = 0;
      long double x_old = 0;
      long double sum_x = 0;
      
      if (width > 1) {
        lambda = arma_weights[n - 2] / arma_weights[n - 1]; // check already passed!
      } else {
        lambda = arma_weights[n - 1];
      }
      
      for (int i = 0; i < n_rows_x; i++) {
        
        if ((arma_any_na[i] != 0) || std::isnan(x(i, j))) {
          
          w_new = 0;
          x_new = 0;
          
        } else {
          
          w_new = arma_weights[n - 1];
          x_new = x(i, j);
          
        }
        
        // expanding window
        if (i < width) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j))) {
            n_obs += 1;
          }
          
          sum_x = lambda * sum_x + w_new * x_new;
          
        }
        
        // rolling window
        if (i >= width) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
              ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)))) {
            
            n_obs += 1;
            
          } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j))) &&
            (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j))) {
            
            n_obs -= 1;
            
          }
          
          if ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j))) {
            
            w_old = 0;
            x_old = 0;
            
          } else {
            
            w_old = arma_weights[0];
            x_old = x(i - width, j);
            
          }
          
          sum_x = lambda * sum_x + w_new * x_new - lambda * w_old * x_old;
          
        }
        
        // don't compute if missing value and 'na_restore' argument is true
        if ((!na_restore) || (na_restore && !std::isnan(x(i, j)))) {
          
          // compute the sum
          if (n_obs >= min_obs) {
            arma_sum(i, j) = sum_x;
          } else {
            arma_sum(i, j) = NA_REAL;
          }
          
        } else {
          
          // can be either NA or NaN
          arma_sum(i, j) = x(i, j);
          
        }
        
      }
      
    }
  }
  
};

// 'Worker' function for computing rolling sums using a standard algorithm
struct RollSumParallel : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_x;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::mat& arma_sum;          // destination (pass by reference)
  
  // initialize with source and destination
  RollSumParallel(const NumericMatrix x, const int n,
                  const int n_rows_x, const int n_cols_x,
                  const int width, const arma::vec arma_weights,
                  const int min_obs, const arma::uvec arma_any_na,
                  const bool na_restore, arma::mat& arma_sum)
    : x(x), n(n),
      n_rows_x(n_rows_x), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      min_obs(min_obs), arma_any_na(arma_any_na),
      na_restore(na_restore), arma_sum(arma_sum) { }
  
  // function call operator that iterates by index
  void operator()(std::size_t begin_index, std::size_t end_index) {
    for (std::size_t z = begin_index; z < end_index; z++) {
      
      // from 1D to 2D array
      int i = z / n_cols_x;
      int j = z % n_cols_x;
      
      int count = 0;
      int n_obs = 0;
      long double sum_x = 0;
      
      // don't compute if missing value and 'na_restore' argument is true
      if ((!na_restore) || (na_restore && !std::isnan(x(i, j)))) {
        
        // number of observations is either the window size or,
        // for partial results, the number of the current row
        while ((width > count) && (i >= count)) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j))) {
            
            // compute the rolling sum
            sum_x += arma_weights[n - count - 1] * x(i - count, j);
            n_obs += 1;
            
          }
          
          count += 1;
          
        }
        
        // compute the sum
        if (n_obs >= min_obs) {
          arma_sum(i, j) = sum_x;
        } else {
          arma_sum(i, j) = NA_REAL;
        }
        
      } else {
        
        // can be either NA or NaN
        arma_sum(i, j) = x(i, j);
        
      }
      
    }
  }
  
};

// 'Worker' function for computing rolling products using a standard algorithm
struct RollProdParallel : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_x;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::mat& arma_prod;         // destination (pass by reference)
  
  // initialize with source and destination
  RollProdParallel(const NumericMatrix x, const int n,
                   const int n_rows_x, const int n_cols_x,
                   const int width, const arma::vec arma_weights,
                   const int min_obs, const arma::uvec arma_any_na,
                   const bool na_restore, arma::mat& arma_prod)
    : x(x), n(n),
      n_rows_x(n_rows_x), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      min_obs(min_obs), arma_any_na(arma_any_na),
      na_restore(na_restore), arma_prod(arma_prod) { }
  
  // function call operator that iterates by index
  void operator()(std::size_t begin_index, std::size_t end_index) {
    for (std::size_t z = begin_index; z < end_index; z++) {
      
      // from 1D to 2D array
      int i = z / n_cols_x;
      int j = z % n_cols_x;
      
      int count = 0;
      int n_obs = 0;
      long double prod_x = 1;
      
      // don't compute if missing value and 'na_restore' argument is true
      if ((!na_restore) || (na_restore && !std::isnan(x(i, j)))) {
        
        // number of observations is either the window size or,
        // for partial results, the number of the current row
        while ((width > count) && (i >= count)) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j))) {
            
            // compute the rolling product
            prod_x *= arma_weights[n - count - 1] * x(i - count, j);
            n_obs += 1;
            
          }
          
          count += 1;
          
        }
        
        // compute the product
        if (n_obs >= min_obs) {
          arma_prod(i, j) = prod_x;
        } else {
          arma_prod(i, j) = NA_REAL;
        }
        
      } else {
        
        // can be either NA or NaN
        arma_prod(i, j) = x(i, j);
        
      }
      
    }
  }
  
};

// 'Worker' function for computing rolling means using an online algorithm
struct RollMeanOnline : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_x;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::mat& arma_mean;         // destination (pass by reference)
  
  // initialize with source and destination
  RollMeanOnline(const NumericMatrix x, const int n,
                 const int n_rows_x, const int n_cols_x,
                 const int width, const arma::vec arma_weights,
                 const int min_obs, const arma::uvec arma_any_na,
                 const bool na_restore, arma::mat& arma_mean)
    : x(x), n(n),
      n_rows_x(n_rows_x), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      min_obs(min_obs), arma_any_na(arma_any_na),
      na_restore(na_restore), arma_mean(arma_mean) { }
  
  // function call operator that iterates by column
  void operator()(std::size_t begin_col, std::size_t end_col) {
    for (std::size_t j = begin_col; j < end_col; j++) {
      
      int n_obs = 0;
      long double lambda = 0;
      long double w_new = 0;
      long double w_old = 0;
      long double x_new = 0;
      long double x_old = 0;
      long double sum_w = 0;
      long double sum_x = 0;
      
      if (width > 1) {
        lambda = arma_weights[n - 2] / arma_weights[n - 1]; // check already passed!
      } else {
        lambda = arma_weights[n - 1];
      }
      
      for (int i = 0; i < n_rows_x; i++) {
        
        if ((arma_any_na[i] != 0) || std::isnan(x(i, j))) {
          
          w_new = 0;
          x_new = 0;
          
        } else {
          
          w_new = arma_weights[n - 1];
          x_new = x(i, j);
          
        }
        
        // expanding window
        if (i < width) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j))) {
            n_obs += 1;
          }
          
          sum_w = lambda * sum_w + w_new;
          sum_x = lambda * sum_x + w_new * x_new;
          
        }
        
        // rolling window
        if (i >= width) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
              ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)))) {
            
            n_obs += 1;
            
          } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j))) &&
            (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j))) {
            
            n_obs -= 1;
            
          }
          
          if ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j))) {
            
            w_old = 0;
            x_old = 0;
            
          } else {
            
            w_old = arma_weights[0];
            x_old = x(i - width, j);
            
          }
          
          sum_w = lambda * sum_w + w_new - lambda * w_old;
          sum_x = lambda * sum_x + w_new * x_new - lambda * w_old * x_old;
          
        }
        
        // don't compute if missing value and 'na_restore' argument is true
        if ((!na_restore) || (na_restore && !std::isnan(x(i, j)))) {
          
          // compute the mean
          if (n_obs >= min_obs) {
            arma_mean(i, j) = sum_x / sum_w;
          } else {
            arma_mean(i, j) = NA_REAL;
          }
          
        } else {
          
          // can be either NA or NaN
          arma_mean(i, j) = x(i, j);
          
        }
        
      }
      
    }
  }
  
};

// 'Worker' function for computing rolling means using a standard algorithm
struct RollMeanParallel : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_x;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::mat& arma_mean;         // destination (pass by reference)
  
  // initialize with source and destination
  RollMeanParallel(const NumericMatrix x, const int n,
                   const int n_rows_x, const int n_cols_x,
                   const int width, const arma::vec arma_weights,
                   const int min_obs, const arma::uvec arma_any_na,
                   const bool na_restore, arma::mat& arma_mean)
    : x(x), n(n),
      n_rows_x(n_rows_x), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      min_obs(min_obs), arma_any_na(arma_any_na),
      na_restore(na_restore), arma_mean(arma_mean) { }
  
  // function call operator that iterates by index
  void operator()(std::size_t begin_index, std::size_t end_index) {
    for (std::size_t z = begin_index; z < end_index; z++) {
      
      // from 1D to 2D array
      int i = z / n_cols_x;
      int j = z % n_cols_x;
      
      int count = 0;
      int n_obs = 0;
      long double sum_w = 0;
      long double sum_x = 0;
      
      // don't compute if missing value and 'na_restore' argument is true
      if ((!na_restore) || (na_restore && !std::isnan(x(i, j)))) {
        
        // number of observations is either the window size or,
        // for partial results, the number of the current row
        while ((width > count) && (i >= count)) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j))) {
            
            // compute the rolling sum
            sum_w += arma_weights[n - count - 1];
            sum_x += arma_weights[n - count - 1] * x(i - count, j);
            n_obs += 1;
            
          }
          
          count += 1;
          
        }
        
        // compute the mean
        if (n_obs >= min_obs) {
          arma_mean(i, j) = sum_x / sum_w;
        } else {
          arma_mean(i, j) = NA_REAL;
        }
        
      } else {
        
        // can be either NA or NaN
        arma_mean(i, j) = x(i, j);
        
      }
      
    }
  }
  
};

// 'Worker' function for computing rolling variances using an online algorithm
struct RollVarOnline : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_x;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const bool center;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::mat& arma_var;          // destination (pass by reference)
  
  // initialize with source and destination
  RollVarOnline(const NumericMatrix x, const int n,
                const int n_rows_x, const int n_cols_x,
                const int width, const arma::vec arma_weights,
                const bool center, const int min_obs,
                const arma::uvec arma_any_na, const bool na_restore,
                arma::mat& arma_var)
    : x(x), n(n),
      n_rows_x(n_rows_x), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      center(center), min_obs(min_obs),
      arma_any_na(arma_any_na), na_restore(na_restore),
      arma_var(arma_var) { }
  
  // function call operator that iterates by column
  void operator()(std::size_t begin_col, std::size_t end_col) {
    for (std::size_t j = begin_col; j < end_col; j++) {
      
      int n_obs = 0;
      long double lambda = 0;
      long double w_new = 0;
      long double w_old = 0; 
      long double x_new = 0;
      long double x_old = 0;
      long double sum_w = 0;
      long double sum_x = 0;
      long double sumsq_w = 0;
      long double sumsq_x = 0;
      long double mean_prev_x = 0;
      long double mean_x = 0;
      
      if (width > 1) {
        lambda = arma_weights[n - 2] / arma_weights[n - 1]; // check already passed!
      } else {
        lambda = arma_weights[n - 1];
      }
      
      for (int i = 0; i < n_rows_x; i++) {
        
        if ((arma_any_na[i] != 0) || std::isnan(x(i, j))) {
          
          w_new = 0;
          x_new = 0;
          
        } else {
          
          w_new = arma_weights[n - 1];
          x_new = x(i, j);
          
        }
        
        // expanding window
        if (i < width) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j))) {
            n_obs += 1;
          }
          
          sum_w = lambda * sum_w + w_new;
          sum_x = lambda * sum_x + w_new * x_new;
          sumsq_w = pow(lambda, (long double)2.0) * sumsq_w + pow(w_new, (long double)2.0);
          
          if (center && (n_obs > 0)) {
            
            // compute the mean
            mean_prev_x = mean_x;
            mean_x = sum_x / sum_w;
            
          }
          
          // compute the sum of squares
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && (n_obs > 1)) {
            
            sumsq_x = lambda * sumsq_x +
              w_new * (x_new - mean_x) * (x_new - mean_prev_x);
            
          } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j))) {
            
            sumsq_x = lambda * sumsq_x;
            
          } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
            (n_obs == 1) && !center) {
            
            sumsq_x = w_new * pow(x_new, (long double)2.0);
            
          }
          
        }
        
        // rolling window
        if (i >= width) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
              ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)))) {
            
            n_obs += 1;
            
          } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j))) &&
            (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j))) {
            
            n_obs -= 1;
            
          }
          
          if ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j))) {
            
            w_old = 0;
            x_old = 0;
            
          } else {
            
            w_old = arma_weights[0];
            x_old = x(i - width, j);
            
          }
          
          sum_w = lambda * sum_w + w_new - lambda * w_old;
          sum_x = lambda * sum_x + w_new * x_new - lambda * w_old * x_old;
          sumsq_w = pow(lambda, (long double)2.0) * sumsq_w +
            pow(w_new, (long double)2.0) - pow(lambda * w_old, (long double)2.0);
          
          if (center && (n_obs > 0)) {
            
            // compute the mean
            mean_prev_x = mean_x;
            mean_x = sum_x / sum_w;
            
          }
          
          // compute the sum of squares
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
              (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j))) {
            
            sumsq_x = lambda * sumsq_x +
              w_new * (x_new - mean_x) * (x_new - mean_prev_x) -
              lambda * w_old * (x_old - mean_x) * (x_old - mean_prev_x);
            
          } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
            ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)))) {
            
            sumsq_x = lambda * sumsq_x +
              w_new * (x_new - mean_x) * (x_new - mean_prev_x);
            
          } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j))) &&
            (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j))) {
            
            sumsq_x = lambda * sumsq_x -
              lambda * w_old * (x_old - mean_x) * (x_old - mean_prev_x);
            
          } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) ||
            (arma_any_na[i - width] != 0) || std::isnan(x(i - width, j))) {
            
            sumsq_x = lambda * sumsq_x;
            
          }
          
        }
        
        // don't compute if missing value and 'na_restore' argument is true
        if ((!na_restore) || (na_restore && !std::isnan(x(i, j)))) {
          
          // compute the unbiased estimate of variance
          if ((n_obs > 1) && (n_obs >= min_obs)) {
            arma_var(i, j) = sumsq_x / (sum_w - sumsq_w / sum_w);
          } else {
            arma_var(i, j) = NA_REAL;
          }
          
        } else {
          
          // can be either NA or NaN
          arma_var(i, j) = x(i, j);
          
        }
        
      }
    }
  }
  
};

// 'Worker' function for computing rolling variances using a standard algorithm
struct RollVarParallel : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_x;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const bool center;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::mat& arma_var;          // destination (pass by reference)
  
  // initialize with source and destination
  RollVarParallel(const NumericMatrix x, const int n,
                  const int n_rows_x, const int n_cols_x,
                  const int width, const arma::vec arma_weights,
                  const bool center, const int min_obs,
                  const arma::uvec arma_any_na, const bool na_restore,
                  arma::mat& arma_var)
    : x(x), n(n),
      n_rows_x(n_rows_x), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      center(center), min_obs(min_obs),
      arma_any_na(arma_any_na), na_restore(na_restore),
      arma_var(arma_var) { }
  
  // function call operator that iterates by index
  void operator()(std::size_t begin_index, std::size_t end_index) {
    for (std::size_t z = begin_index; z < end_index; z++) {
      
      // from 1D to 2D array
      int i = z / n_cols_x;
      int j = z % n_cols_x;
      
      long double mean_x = 0;
      
      // don't compute if missing value and 'na_restore' argument is true
      if ((!na_restore) || (na_restore && !std::isnan(x(i, j)))) {
        
        if (center) {
          
          int count = 0;
          long double sum_w = 0;
          long double sum_x = 0;
          
          // number of observations is either the window size or,
          // for partial results, the number of the current row
          while ((width > count) && (i >= count)) {
            
            // don't include if missing value and 'any_na' argument is 1
            // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
            if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j))) {
              
              // compute the rolling sum
              sum_w += arma_weights[n - count - 1];
              sum_x += arma_weights[n - count - 1] * x(i - count, j);
              
            }
            
            count += 1;
            
          }
          
          // compute the mean
          mean_x = sum_x / sum_w;
          
        }
        
        int count = 0;
        int n_obs = 0;
        long double sum_w = 0;
        long double sumsq_w = 0;
        long double sumsq_x = 0;
        
        // number of observations is either the window size or,
        // for partial results, the number of the current row
        while ((width > count) && (i >= count)) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j))) {
            
            sum_w += arma_weights[n - count - 1];
            sumsq_w += pow(arma_weights[n - count - 1], 2.0);
            
            // compute the rolling sum of squares with 'center' argument
            if (center) {
              sumsq_x += arma_weights[n - count - 1] *
                pow(x(i - count, j) - mean_x, (long double)2.0);
            } else if (!center) {
              sumsq_x += arma_weights[n - count - 1] *
                pow(x(i - count, j), 2.0);
            }
            
            n_obs += 1;
            
          }
          
          count += 1;
          
        }
        
        // compute the unbiased estimate of variance
        if ((n_obs > 1) && (n_obs >= min_obs)) {
          arma_var(i, j) = sumsq_x / (sum_w - sumsq_w / sum_w);
        } else {
          arma_var(i, j) = NA_REAL;
        }
        
      } else {
        
        // can be either NA or NaN
        arma_var(i, j) = x(i, j);
        
      }
      
    }
  }
  
};

// 'Worker' function for computing rolling standard deviations using an online algorithm
struct RollSdOnline : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_x;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const bool center;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::mat& arma_sd;         // destination (pass by reference)
  
  // initialize with source and destination
  RollSdOnline(const NumericMatrix x, const int n,
               const int n_rows_x, const int n_cols_x,
               const int width, const arma::vec arma_weights,
               const bool center, const int min_obs,
               const arma::uvec arma_any_na, const bool na_restore,
               arma::mat& arma_sd)
    : x(x), n(n),
      n_rows_x(n_rows_x), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      center(center), min_obs(min_obs),
      arma_any_na(arma_any_na), na_restore(na_restore),
      arma_sd(arma_sd) { }
  
  // function call operator that iterates by column
  void operator()(std::size_t begin_col, std::size_t end_col) {
    for (std::size_t j = begin_col; j < end_col; j++) {
      
      int n_obs = 0;
      long double lambda = 0;
      long double w_new = 0;
      long double w_old = 0; 
      long double x_new = 0;
      long double x_old = 0;
      long double sum_w = 0;
      long double sum_x = 0;
      long double sumsq_w = 0;
      long double sumsq_x = 0;
      long double mean_prev_x = 0;
      long double mean_x = 0;
      
      if (width > 1) {
        lambda = arma_weights[n - 2] / arma_weights[n - 1]; // check already passed!
      } else {
        lambda = arma_weights[n - 1];
      }
      
      for (int i = 0; i < n_rows_x; i++) {
        
        if ((arma_any_na[i] != 0) || std::isnan(x(i, j))) {
          
          w_new = 0;
          x_new = 0;
          
        } else {
          
          w_new = arma_weights[n - 1];
          x_new = x(i, j);
          
        }
        
        // expanding window
        if (i < width) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j))) {
            n_obs += 1;
          }
          
          sum_w = lambda * sum_w + w_new;
          sum_x = lambda * sum_x + w_new * x_new;
          sumsq_w = pow(lambda, (long double)2.0) * sumsq_w + pow(w_new, (long double)2.0);
          
          if (center && (n_obs > 0)) {
            
            // compute the mean
            mean_prev_x = mean_x;
            mean_x = sum_x / sum_w;
            
          }
          
          // compute the sum of squares
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && (n_obs > 1)) {
            
            sumsq_x = lambda * sumsq_x +
              w_new * (x_new - mean_x) * (x_new - mean_prev_x);
            
          } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j))) {
            
            sumsq_x = lambda * sumsq_x;
            
          } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
            (n_obs == 1) && !center) {
            
            sumsq_x = w_new * pow(x_new, (long double)2.0);
            
          }
          
        }
        
        // rolling window
        if (i >= width) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
              ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)))) {
            
            n_obs += 1;
            
          } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j))) &&
            (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j))) {
            
            n_obs -= 1;
            
          }
          
          if ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j))) {
            
            w_old = 0;
            x_old = 0;
            
          } else {
            
            w_old = arma_weights[0];
            x_old = x(i - width, j);
            
          }
          
          sum_w = lambda * sum_w + w_new - lambda * w_old;
          sum_x = lambda * sum_x + w_new * x_new - lambda * w_old * x_old;
          sumsq_w = pow(lambda, (long double)2.0) * sumsq_w +
            pow(w_new, (long double)2.0) - pow(lambda * w_old, (long double)2.0);
          
          if (center && (n_obs > 0)) {
            
            // compute the mean
            mean_prev_x = mean_x;
            mean_x = sum_x / sum_w;
            
          }
          
          // compute the sum of squares
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
              (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j))) {
            
            sumsq_x = lambda * sumsq_x +
              w_new * (x_new - mean_x) * (x_new - mean_prev_x) -
              lambda * w_old * (x_old - mean_x) * (x_old - mean_prev_x);
            
          } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
            ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)))) {
            
            sumsq_x = lambda * sumsq_x +
              w_new * (x_new - mean_x) * (x_new - mean_prev_x);
            
          } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j))) &&
            (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j))) {
            
            sumsq_x = lambda * sumsq_x -
              lambda * w_old * (x_old - mean_x) * (x_old - mean_prev_x);
            
          } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) ||
            (arma_any_na[i - width] != 0) || std::isnan(x(i - width, j))) {
            
            sumsq_x = lambda * sumsq_x;
            
          }
          
        }
        
        // don't compute if missing value and 'na_restore' argument is true
        if ((!na_restore) || (na_restore && !std::isnan(x(i, j)))) {
          
          // compute the unbiased estimate of standard deviation
          if ((n_obs > 1) && (n_obs >= min_obs)) {
            arma_sd(i, j) = sqrt(sumsq_x / (sum_w - sumsq_w / sum_w));
          } else {
            arma_sd(i, j) = NA_REAL;
          }
          
        } else {
          
          // can be either NA or NaN
          arma_sd(i, j) = x(i, j);
          
        }
        
      }
    }
  }
  
};

// 'Worker' function for computing rolling standard deviations using a standard algorithm
struct RollSdParallel : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_x;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const bool center;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::mat& arma_sd;           // destination (pass by reference)
  
  // initialize with source and destination
  RollSdParallel(const NumericMatrix x, const int n,
                 const int n_rows_x, const int n_cols_x,
                 const int width, const arma::vec arma_weights,
                 const bool center, const int min_obs,
                 const arma::uvec arma_any_na, const bool na_restore,
                 arma::mat& arma_sd)
    : x(x), n(n),
      n_rows_x(n_rows_x), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      center(center), min_obs(min_obs),
      arma_any_na(arma_any_na), na_restore(na_restore),
      arma_sd(arma_sd) { }
  
  // function call operator that iterates by index
  void operator()(std::size_t begin_index, std::size_t end_index) {
    for (std::size_t z = begin_index; z < end_index; z++) {
      
      // from 1D to 2D array
      int i = z / n_cols_x;
      int j = z % n_cols_x;
      
      long double mean_x = 0;
      
      // don't compute if missing value and 'na_restore' argument is true
      if ((!na_restore) || (na_restore && !std::isnan(x(i, j)))) {
        
        if (center) {
          
          int count = 0;
          long double sum_w = 0;
          long double sum_x = 0;
          
          // number of observations is either the window size or,
          // for partial results, the number of the current row
          while ((width > count) && (i >= count)) {
            
            // don't include if missing value and 'any_na' argument is 1
            // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
            if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j))) {
              
              // compute the rolling sum
              sum_w += arma_weights[n - count - 1];
              sum_x += arma_weights[n - count - 1] * x(i - count, j);
              
            }
            
            count += 1;
            
          }
          
          // compute the mean
          mean_x = sum_x / sum_w;
          
        }
        
        int count = 0;
        int n_obs = 0;
        long double sum_w = 0;
        long double sumsq_w = 0;
        long double sumsq_x = 0;
        
        // number of observations is either the window size or,
        // for partial results, the number of the current row
        while ((width > count) && (i >= count)) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j))) {
            
            sum_w += arma_weights[n - count - 1];
            sumsq_w += pow(arma_weights[n - count - 1], 2.0);
            
            // compute the rolling sum of squares with 'center' argument
            if (center) {
              sumsq_x += arma_weights[n - count - 1] *
                pow(x(i - count, j) - mean_x, (long double)2.0);
            } else if (!center) {
              sumsq_x += arma_weights[n - count - 1] *
                pow(x(i - count, j), 2.0);
            }
            
            n_obs += 1;
            
          }
          
          count += 1;
          
        }
        
        // compute the unbiased estimate of standard deviation
        if ((n_obs > 1) && (n_obs >= min_obs)) {
          arma_sd(i, j) = sqrt(sumsq_x / (sum_w - sumsq_w / sum_w));
        } else {
          arma_sd(i, j) = NA_REAL;
        }
        
      } else {
        
        // can be either NA or NaN
        arma_sd(i, j) = x(i, j);
        
      }
      
    }
  }
  
};

// 'Worker' function for computing rolling centering and scaling using an online algorithm
struct RollScaleOnline : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_x;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const bool center;
  const bool scale;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::mat& arma_scale;        // destination (pass by reference)
  
  // initialize with source and destination
  RollScaleOnline(const NumericMatrix x, const int n,
                  const int n_rows_x, const int n_cols_x,
                  const int width, const arma::vec arma_weights,
                  const bool center, const bool scale,
                  const int min_obs, const arma::uvec arma_any_na,
                  const bool na_restore, arma::mat& arma_scale)
    : x(x), n(n),
      n_rows_x(n_rows_x), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      center(center), scale(scale),
      min_obs(min_obs), arma_any_na(arma_any_na),
      na_restore(na_restore), arma_scale(arma_scale) { }
  
  // function call operator that iterates by column
  void operator()(std::size_t begin_col, std::size_t end_col) {
    for (std::size_t j = begin_col; j < end_col; j++) {
      
      int n_obs = 0;
      long double lambda = 0;
      long double w_new = 0;
      long double w_old = 0; 
      long double x_new = 0;
      long double x_old = 0;
      long double sum_w = 0;
      long double sum_x = 0;
      long double sumsq_w = 0;
      long double sumsq_x = 0;
      long double mean_prev_x = 0;
      long double mean_x = 0;
      long double var_x = 0;
      long double x_ij = 0;
      
      if (width > 1) {
        lambda = arma_weights[n - 2] / arma_weights[n - 1]; // check already passed!
      } else {
        lambda = arma_weights[n - 1];
      }
      
      for (int i = 0; i < n_rows_x; i++) {
        
        if ((arma_any_na[i] != 0) || std::isnan(x(i, j))) {
          
          w_new = 0;
          x_new = 0;
          
        } else {
          
          w_new = arma_weights[n - 1];
          x_new = x(i, j);
          x_ij = x(i, j);
          
        }
        
        // expanding window
        if (i < width) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j))) {
            n_obs += 1;
          }
          
          sum_w = lambda * sum_w + w_new;
          sum_x = lambda * sum_x + w_new * x_new;
          sumsq_w = pow(lambda, (long double)2.0) * sumsq_w + pow(w_new, (long double)2.0);
          
          if (center && (n_obs > 0)) {
            
            // compute the mean
            mean_prev_x = mean_x;
            mean_x = sum_x / sum_w;
            
          }
          
          if (scale) {
            
            // compute the sum of squares
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && (n_obs > 1)) {
              
              sumsq_x = lambda * sumsq_x +
                w_new * (x_new - mean_x) * (x_new - mean_prev_x);
              
            } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j))) {
              
              sumsq_x = lambda * sumsq_x;
              
            } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
              (n_obs == 1) && !center) {
              
              sumsq_x = w_new * pow(x_new, (long double)2.0);
              
            }
            
            var_x = sumsq_x / (sum_w - sumsq_w / sum_w);
            
          }
          
        }
        
        // rolling window
        if (i >= width) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
              ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)))) {
            
            n_obs += 1;
            
          } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j))) &&
            (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j))) {
            
            n_obs -= 1;
            
          }
          
          if ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j))) {
            
            w_old = 0;
            x_old = 0;
            
          } else {
            
            w_old = arma_weights[0];
            x_old = x(i - width, j);
            
          }
          
          sum_w = lambda * sum_w + w_new - lambda * w_old;
          sum_x = lambda * sum_x + w_new * x_new - lambda * w_old * x_old;
          sumsq_w = pow(lambda, (long double)2.0) * sumsq_w +
            pow(w_new, (long double)2.0) - pow(lambda * w_old, (long double)2.0);
          
          if (center && (n_obs > 0)) {
            
            // compute the mean
            mean_prev_x = mean_x;
            mean_x = sum_x / sum_w;
            
          }
          
          if (scale) {
            
            // compute the sum of squares
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
                (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j))) {
              
              sumsq_x = lambda * sumsq_x +
                w_new * (x_new - mean_x) * (x_new - mean_prev_x) -
                lambda * w_old * (x_old - mean_x) * (x_old - mean_prev_x);
              
            } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) &&
              ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)))) {
              
              sumsq_x = lambda * sumsq_x +
                w_new * (x_new - mean_x) * (x_new - mean_prev_x);
              
            } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j))) &&
              (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j))) {
              
              sumsq_x = lambda * sumsq_x -
                lambda * w_old * (x_old - mean_x) * (x_old - mean_prev_x);
              
            } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) ||
              (arma_any_na[i - width] != 0) || std::isnan(x(i - width, j))) {
              
              sumsq_x = lambda * sumsq_x;
              
            }
            
            var_x = sumsq_x / (sum_w - sumsq_w / sum_w);
            
          }
          
        }
        
        // don't compute if missing value and 'na_restore' argument is true
        if ((!na_restore) || (na_restore && !std::isnan(x(i, j)))) {
          
          // compute the unbiased estimate of centering and scaling
          if (n_obs >= min_obs) {
            
            if (scale && ((n_obs <= 1) ||
                sqrt(var_x) <= sqrt(arma::datum::eps))) {
              arma_scale(i, j) = NA_REAL;
            } else if (center && scale) {
              arma_scale(i, j) = (x_ij - mean_x) / sqrt(var_x);
            } else if (!center && scale) {
              arma_scale(i, j) = x_ij / sqrt(var_x);
            } else if (center && !scale) {
              arma_scale(i, j) = x_ij - mean_x;
            } else if (!center && !scale) {
              arma_scale(i, j) = x_ij;
            }
            
          } else {
            arma_scale(i, j) = NA_REAL;
          }
          
        } else {
          
          // can be either NA or NaN
          arma_scale(i, j) = x(i, j);
          
        }
        
      }
    }
  }
  
};

// 'Worker' function for computing rolling centering and scaling using a standard algorithm
struct RollScaleParallel : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_x;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const bool center;
  const bool scale;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::mat& arma_scale;        // destination (pass by reference)
  
  // initialize with source and destination
  RollScaleParallel(const NumericMatrix x, const int n,
                    const int n_rows_x, const int n_cols_x,
                    const int width, const arma::vec arma_weights,
                    const bool center, const bool scale,
                    const int min_obs, const arma::uvec arma_any_na,
                    const bool na_restore, arma::mat& arma_scale)
    : x(x), n(n),
      n_rows_x(n_rows_x), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      center(center), scale(scale),
      min_obs(min_obs), arma_any_na(arma_any_na),
      na_restore(na_restore), arma_scale(arma_scale) { }
  
  // function call operator that iterates by index
  void operator()(std::size_t begin_index, std::size_t end_index) {
    for (std::size_t z = begin_index; z < end_index; z++) {
      
      // from 1D to 2D array
      int i = z / n_cols_x;
      int j = z % n_cols_x;
      
      long double mean_x = 0;
      long double var_x = 0;
      
      // don't compute if missing value and 'na_restore' argument is true
      if ((!na_restore) || (na_restore && !std::isnan(x(i, j)))) {
        
        if (center) {
          
          int count = 0;
          long double sum_w = 0;
          long double sum_x = 0;
          
          // number of observations is either the window size or,
          // for partial results, the number of the current row
          while ((width > count) && (i >= count)) {
            
            // don't include if missing value and 'any_na' argument is 1
            // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
            if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j))) {
              
              // compute the rolling sum
              sum_w += arma_weights[n - count - 1];
              sum_x += arma_weights[n - count - 1] * x(i - count, j);
              
            }
            
            count += 1;
            
          }
          
          // compute the mean
          mean_x = sum_x / sum_w;
          
        }
        
        if (scale) {
          
          int count = 0;
          int n_obs = 0;
          long double sum_w = 0;
          long double sumsq_w = 0;
          long double sumsq_x = 0;
          
          // number of observations is either the window size or,
          // for partial results, the number of the current row
          while ((width > count) && (i >= count)) {
            
            // don't include if missing value and 'any_na' argument is 1
            // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
            if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j))) {
              
              sum_w += arma_weights[n - count - 1];
              sumsq_w += pow(arma_weights[n - count - 1], 2.0);
              
              // compute the rolling sum of squares with 'center' argument
              if (center) {
                sumsq_x += arma_weights[n - count - 1] *
                  pow(x(i - count, j) - mean_x, (long double)2.0);
              } else if (!center) {
                sumsq_x += arma_weights[n - count - 1] *
                  pow(x(i - count, j), 2.0);
              }
              
              n_obs += 1;
              
            }
            
            count += 1;
            
          }
          
          // compute the unbiased estimate of variance
          var_x = sumsq_x / (sum_w - sumsq_w / sum_w);
          
        }
        
        int count = 0;
        int n_obs = 0;
        bool any_na = false;
        long double x_ij = 0;
        
        // number of observations is either the window size or,
        // for partial results, the number of the current row
        while ((width > count) && (i >= count)) {
          
          // don't include if missing value and 'any_na' argument is 1
          // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
          if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j))) {
            
            // keep first non-missing value
            if (!any_na) {
              x_ij = x(i - count, j);
            }
            
            any_na = true;
            n_obs += 1;
            
          }
          
          count += 1;
          
        }
        
        // compute the unbiased estimate of centering and scaling
        if (n_obs >= min_obs) {
          
          if (scale && ((n_obs <= 1) ||
              sqrt(var_x) <= sqrt(arma::datum::eps))) {
            arma_scale(i, j) = NA_REAL;
          } else if (center && scale) {
            arma_scale(i, j) = (x_ij - mean_x) / sqrt(var_x);
          } else if (!center && scale) {
            arma_scale(i, j) = x_ij / sqrt(var_x);
          } else if (center && !scale) {
            arma_scale(i, j) = x_ij - mean_x;
          } else if (!center && !scale) {
            arma_scale(i, j) = x_ij;
          }
          
        } else {
          arma_scale(i, j) = NA_REAL;
        }
        
      } else {
        
        // can be either NA or NaN
        arma_scale(i, j) = x(i, j);
        
      }
      
    }
  }
  
};

// 'Worker' function for computing rolling covariances using an online algorithm
struct RollCovOnlineXX : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_xy;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const bool center;
  const bool scale;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::cube& arma_cov;         // destination (pass by reference)
  
  // initialize with source and destination
  RollCovOnlineXX(const NumericMatrix x, const int n,
                  const int n_rows_xy, const int n_cols_x,
                  const int width, const arma::vec arma_weights,
                  const bool center, const bool scale,
                  const int min_obs, const arma::uvec arma_any_na,
                  const bool na_restore, arma::cube& arma_cov)
    : x(x), n(n),
      n_rows_xy(n_rows_xy), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      center(center), scale(scale),
      min_obs(min_obs), arma_any_na(arma_any_na),
      na_restore(na_restore), arma_cov(arma_cov) { }
  
  // function call operator that iterates by column
  void operator()(std::size_t begin_col, std::size_t end_col) {
    for (std::size_t j = begin_col; j < end_col; j++) {
      for (std::size_t k = 0; k <= j; k++) {
        
        int n_obs = 0;
        long double lambda = 0;
        long double w_new = 0;
        long double w_old = 0;      
        long double x_new = 0;
        long double x_old = 0;
        long double y_new = 0;
        long double y_old = 0;
        long double sum_w = 0;
        long double sum_x = 0;
        long double sum_y = 0;
        long double sumsq_w = 0;
        long double sumsq_x = 0;
        long double sumsq_y = 0;
        long double sumsq_xy = 0;
        long double mean_prev_x = 0;
        long double mean_prev_y = 0;
        long double mean_x = 0;
        long double mean_y = 0;
        
        if (width > 1) {
          lambda = arma_weights[n - 2] / arma_weights[n - 1]; // check already passed!
        } else {
          lambda = arma_weights[n - 1];
        }
        
        for (int i = 0; i < n_rows_xy; i++) {
          
          if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(x(i, k))) {
            
            w_new = 0;
            x_new = 0;
            y_new = 0;
            
          } else {
            
            w_new = arma_weights[n - 1];
            x_new = x(i, j);
            y_new = x(i, k);
            
          }
          
          // expanding window
          if (i < width) {
            
            // don't include if missing value and 'any_na' argument is 1
            // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k))) {
              n_obs += 1;
            }
            
            sum_w = lambda * sum_w + w_new;
            sum_x = lambda * sum_x + w_new * x_new;
            sum_y = lambda * sum_y + w_new * y_new;
            sumsq_w = pow(lambda, (long double)2.0) * sumsq_w + pow(w_new, (long double)2.0);
            
            if (center && (n_obs > 0)) {
              
              // compute the mean
              mean_prev_x = mean_x;
              mean_prev_y = mean_y;
              mean_x = sum_x / sum_w;
              mean_y = sum_y / sum_w;
              
            }
            
            if (scale) {
              
              // compute the sum of squares
              if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) &&
                  (n_obs > 1)) {
                
                sumsq_x = lambda * sumsq_x +
                  w_new * (x_new - mean_x) * (x_new - mean_prev_x);
                sumsq_y = lambda * sumsq_y +
                  w_new * (y_new - mean_y) * (y_new - mean_prev_y);
                
              } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(x(i, k))) {
                
                sumsq_x = lambda * sumsq_x;
                sumsq_y = lambda * sumsq_y;
                
              } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) &&
                (n_obs == 1) && !center) {
                
                sumsq_x = w_new * pow(x_new, (long double)2.0);
                sumsq_y = w_new * pow(y_new, (long double)2.0);
                
              }
              
            }
            
            // compute the sum of squares
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) &&
                (n_obs > 1)) {
              
              sumsq_xy = lambda * sumsq_xy +
                w_new * (x_new - mean_x) * (y_new - mean_prev_y);
              
            } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(x(i, k))) {
              
              sumsq_xy = lambda * sumsq_xy;
              
            } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) &&
              (n_obs == 1) && !center) {
              
              sumsq_xy = w_new * x_new * y_new;
              
            }
            
          }
          
          // rolling window
          if (i >= width) {
            
            // don't include if missing value and 'any_na' argument is 1
            // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) &&
                ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(x(i - width, k)))) {
              
              n_obs += 1;
              
            } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(x(i, k))) &&
              (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j)) && !std::isnan(x(i - width, k))) {
              
              n_obs -= 1;
              
            }
            
            if ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(x(i - width, k))) {
              
              w_old = 0;
              x_old = 0;
              y_old = 0;
              
            } else {
              
              w_old = arma_weights[0];
              x_old = x(i - width, j);
              y_old = x(i - width, k);
              
            }
            
            sum_w = lambda * sum_w + w_new - lambda * w_old;
            sum_x = lambda * sum_x + w_new * x_new - lambda * w_old * x_old;
            sum_y = lambda * sum_y + w_new * y_new - lambda * w_old * y_old;
            sumsq_w = pow(lambda, (long double)2.0) * sumsq_w +
              pow(w_new, (long double)2.0) - pow(lambda * w_old, (long double)2.0);
            
            if (center && (n_obs > 0)) {
              
              // compute the mean
              mean_prev_x = mean_x;
              mean_prev_y = mean_y;
              mean_x = sum_x / sum_w;
              mean_y = sum_y / sum_w;
              
            }
            
            if (scale) {
              
              // compute the sum of squares
              if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) &&
                  (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j)) && !std::isnan(x(i - width, k))) {
                
                sumsq_x = lambda * sumsq_x +
                  w_new * (x_new - mean_x) * (x_new - mean_prev_x) -
                  lambda * w_old * (x_old - mean_x) * (x_old - mean_prev_x);
                sumsq_y = lambda * sumsq_y +
                  w_new * (y_new - mean_y) * (y_new - mean_prev_y) -
                  lambda * w_old * (y_old - mean_y) * (y_old - mean_prev_y);
                
              } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) &&
                ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(x(i - width, k)))) {
                
                sumsq_x = lambda * sumsq_x +
                  w_new * (x_new - mean_x) * (x_new - mean_prev_x);
                sumsq_y = lambda * sumsq_y +
                  w_new * (y_new - mean_y) * (y_new - mean_prev_y);
                
              } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(x(i, k))) &&
                (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j)) && !std::isnan(x(i - width, k))) {
                
                sumsq_x = lambda * sumsq_x -
                  lambda * w_old * (x_old - mean_x) * (x_old - mean_prev_x);
                sumsq_y = lambda * sumsq_y -
                  lambda * w_old * (y_old - mean_y) * (y_old - mean_prev_y);
                
              } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(x(i, k)) ||
                (arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(x(i - width, k))) {
                
                sumsq_x = lambda * sumsq_x;
                sumsq_y = lambda * sumsq_y;
                
              }
              
            }
            
            // compute the sum of squares
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) &&
                (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j)) && !std::isnan(x(i - width, k))) {
              
              sumsq_xy = lambda * sumsq_xy +
                w_new * (x_new - mean_x) * (y_new - mean_prev_y) -
                lambda * w_old * (x_old - mean_x) * (y_old - mean_prev_y);
              
            } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) &&
              ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(x(i - width, k)))) {
              
              sumsq_xy = lambda * sumsq_xy +
                w_new * (x_new - mean_x) * (y_new - mean_prev_y);
              
            } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(x(i, k))) &&
              (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j)) && !std::isnan(x(i - width, k))) {
              
              sumsq_xy = lambda * sumsq_xy -
                lambda * w_old * (x_old - mean_x) * (y_old - mean_prev_y);
              
            } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(x(i, k)) ||
              (arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(x(i - width, k))) {
              
              sumsq_xy = lambda * sumsq_xy;
              
            }
            
          }
          
          // don't compute if missing value and 'na_restore' argument is true
          if ((!na_restore) || (na_restore && !std::isnan(x(i, j)) &&
              !std::isnan(x(i, k)))) {
              
              // compute the unbiased estimate of variance
              if ((n_obs > 1) && (n_obs >= min_obs)) {
                
                if (scale) {
                  
                  // don't compute if the standard deviation is zero
                  if ((sqrt(sumsq_x) <= sqrt(arma::datum::eps)) ||
                      (sqrt(sumsq_y) <= sqrt(arma::datum::eps))) {
                    arma_cov(j, k, i) = NA_REAL;
                  } else {
                    arma_cov(j, k, i) = sumsq_xy / (sqrt(sumsq_x) * sqrt(sumsq_y));
                  }
                  
                } else if (!scale) {
                  arma_cov(j, k, i) = sumsq_xy / (sum_w - sumsq_w / sum_w);
                }
                
              } else {
                arma_cov(j, k, i) = NA_REAL;
              }
              
          } else {
            
            // can be either NA or NaN
            if (std::isnan(x(i, j))) {
              arma_cov(j, k, i) = x(i, j);
            } else {
              arma_cov(j, k, i) = x(i, k);
            }
            
          }
          
          // covariance matrix is symmetric
          arma_cov(k, j, i) = arma_cov(j, k, i);
          
        }
        
      }
    }
  }
  
};

// 'Worker' function for computing rolling covariances using an online algorithm
struct RollCovOnlineXY : public Worker {
  
  const RMatrix<double> x;      // source
  const RMatrix<double> y;      // source
  const int n;
  const int n_rows_xy;
  const int n_cols_x;
  const int n_cols_y;
  const int width;
  const arma::vec arma_weights;
  const bool center;
  const bool scale;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::cube& arma_cov;         // destination (pass by reference)
  
  // initialize with source and destination
  RollCovOnlineXY(const NumericMatrix x, const NumericMatrix y,
                  const int n, const int n_rows_xy,
                  const int n_cols_x, const int n_cols_y,
                  const int width, const arma::vec arma_weights,
                  const bool center, const bool scale,
                  const int min_obs, const arma::uvec arma_any_na,
                  const bool na_restore, arma::cube& arma_cov)
    : x(x), y(y),
      n(n), n_rows_xy(n_rows_xy),
      n_cols_x(n_cols_x), n_cols_y(n_cols_y),
      width(width), arma_weights(arma_weights),
      center(center), scale(scale),
      min_obs(min_obs), arma_any_na(arma_any_na),
      na_restore(na_restore), arma_cov(arma_cov) { }
  
  // function call operator that iterates by column
  void operator()(std::size_t begin_col, std::size_t end_col) {
    for (std::size_t j = begin_col; j < end_col; j++) {
      for (int k = 0; k <= n_cols_y - 1; k++) {
        
        int n_obs = 0;
        long double lambda = 0;
        long double w_new = 0;
        long double w_old = 0;      
        long double x_new = 0;
        long double x_old = 0;
        long double y_new = 0;
        long double y_old = 0;
        long double sum_w = 0;
        long double sum_x = 0;
        long double sum_y = 0;
        long double sumsq_w = 0;
        long double sumsq_x = 0;
        long double sumsq_y = 0;
        long double sumsq_xy = 0;
        long double mean_prev_x = 0;
        long double mean_prev_y = 0;
        long double mean_x = 0;
        long double mean_y = 0;
        
        if (width > 1) {
          lambda = arma_weights[n - 2] / arma_weights[n - 1]; // check already passed!
        } else {
          lambda = arma_weights[n - 1];
        }
        
        for (int i = 0; i < n_rows_xy; i++) {
          
          if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(y(i, k))) {
            
            w_new = 0;
            x_new = 0;
            y_new = 0;
            
          } else {
            
            w_new = arma_weights[n - 1];
            x_new = x(i, j);
            y_new = y(i, k);
            
          }
          
          // expanding window
          if (i < width) {
            
            // don't include if missing value and 'any_na' argument is 1
            // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(y(i, k))) {
              n_obs += 1;
            }
            
            sum_w = lambda * sum_w + w_new;
            sum_x = lambda * sum_x + w_new * x_new;
            sum_y = lambda * sum_y + w_new * y_new;
            sumsq_w = pow(lambda, (long double)2.0) * sumsq_w + pow(w_new, (long double)2.0);
            
            if (center && (n_obs > 0)) {
              
              // compute the mean
              mean_prev_x = mean_x;
              mean_prev_y = mean_y;
              mean_x = sum_x / sum_w;
              mean_y = sum_y / sum_w;
              
            }
            
            if (scale) {
              
              // compute the sum of squares
              if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(y(i, k)) &&
                  (n_obs > 1)) {
                
                sumsq_x = lambda * sumsq_x +
                  w_new * (x_new - mean_x) * (x_new - mean_prev_x);
                sumsq_y = lambda * sumsq_y +
                  w_new * (y_new - mean_y) * (y_new - mean_prev_y);
                
              } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(y(i, k))) {
                
                sumsq_x = lambda * sumsq_x;
                sumsq_y = lambda * sumsq_y;
                
              } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(y(i, k)) &&
                (n_obs == 1) && !center) {
                
                sumsq_x = w_new * pow(x_new, (long double)2.0);
                sumsq_y = w_new * pow(y_new, (long double)2.0);
                
              }
              
            }
            
            // compute the sum of squares
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(y(i, k)) &&
                (n_obs > 1)) {
              
              sumsq_xy = lambda * sumsq_xy +
                w_new * (x_new - mean_x) * (y_new - mean_prev_y);
              
            } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(y(i, k))) {
              
              sumsq_xy = lambda * sumsq_xy;
              
            } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(y(i, k)) &&
              (n_obs == 1) && !center) {
              
              sumsq_xy = w_new * x_new * y_new;
              
            }
            
          }
          
          // rolling window
          if (i >= width) {
            
            // don't include if missing value and 'any_na' argument is 1
            // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(y(i, k)) &&
                ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(y(i - width, k)))) {
              
              n_obs += 1;
              
            } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(y(i, k))) &&
              (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j)) && !std::isnan(y(i - width, k))) {
              
              n_obs -= 1;
              
            }
            
            if ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(y(i - width, k))) {
              
              w_old = 0;
              x_old = 0;
              y_old = 0;
              
            } else {
              
              w_old = arma_weights[0];
              x_old = x(i - width, j);
              y_old = y(i - width, k);
              
            }
            
            sum_w = lambda * sum_w + w_new - lambda * w_old;
            sum_x = lambda * sum_x + w_new * x_new - lambda * w_old * x_old;
            sum_y = lambda * sum_y + w_new * y_new - lambda * w_old * y_old;
            sumsq_w = pow(lambda, (long double)2.0) * sumsq_w +
              pow(w_new, (long double)2.0) - pow(lambda * w_old, (long double)2.0);
            
            if (center && (n_obs > 0)) {
              
              // compute the mean
              mean_prev_x = mean_x;
              mean_prev_y = mean_y;
              mean_x = sum_x / sum_w;
              mean_y = sum_y / sum_w;
              
            }
            
            if (scale) {
              
              // compute the sum of squares
              if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(y(i, k)) &&
                  (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j)) && !std::isnan(y(i - width, k))) {
                
                sumsq_x = lambda * sumsq_x +
                  w_new * (x_new - mean_x) * (x_new - mean_prev_x) -
                  lambda * w_old * (x_old - mean_x) * (x_old - mean_prev_x);
                sumsq_y = lambda * sumsq_y +
                  w_new * (y_new - mean_y) * (y_new - mean_prev_y) -
                  lambda * w_old * (y_old - mean_y) * (y_old - mean_prev_y);
                
              } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(y(i, k)) &&
                ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(y(i - width, k)))) {
                
                sumsq_x = lambda * sumsq_x +
                  w_new * (x_new - mean_x) * (x_new - mean_prev_x);
                sumsq_y = lambda * sumsq_y +
                  w_new * (y_new - mean_y) * (y_new - mean_prev_y);
                
              } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(y(i, k))) &&
                (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j)) && !std::isnan(y(i - width, k))) {
                
                sumsq_x = lambda * sumsq_x -
                  lambda * w_old * (x_old - mean_x) * (x_old - mean_prev_x);
                sumsq_y = lambda * sumsq_y -
                  lambda * w_old * (y_old - mean_y) * (y_old - mean_prev_y);
                
              } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(y(i, k)) ||
                (arma_any_na[i - width] == 0) || std::isnan(x(i - width, j)) || std::isnan(y(i - width, k))) {
                
                sumsq_x = lambda * sumsq_x;
                sumsq_y = lambda * sumsq_y;
                
              }
              
            }
            
            // compute the sum of squares
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(y(i, k)) &&
                (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j)) && !std::isnan(y(i - width, k))) {
              
              sumsq_xy = lambda * sumsq_xy +
                w_new * (x_new - mean_x) * (y_new - mean_prev_y) -
                lambda * w_old * (x_old - mean_x) * (y_old - mean_prev_y);
              
            } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(y(i, k)) &&
              ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(y(i - width, k)))) {
              
              sumsq_xy = lambda * sumsq_xy +
                w_new * (x_new - mean_x) * (y_new - mean_prev_y);
              
            } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(y(i, k))) &&
              (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j)) && !std::isnan(y(i - width, k))) {
              
              sumsq_xy = lambda * sumsq_xy -
                lambda * w_old * (x_old - mean_x) * (y_old - mean_prev_y);
              
            } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(y(i, k)) ||
              (arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(y(i - width, k))) {
              
              sumsq_xy = lambda * sumsq_xy;
              
            }
            
          }
          
          // don't compute if missing value and 'na_restore' argument is true
          if ((!na_restore) || (na_restore && !std::isnan(x(i, j)) &&
              !std::isnan(y(i, k)))) {
              
              // compute the unbiased estimate of variance
              if ((n_obs > 1) && (n_obs >= min_obs)) {
                
                if (scale) {
                  
                  // don't compute if the standard deviation is zero
                  if ((sqrt(sumsq_x) <= sqrt(arma::datum::eps)) ||
                      (sqrt(sumsq_y) <= sqrt(arma::datum::eps))) {
                    arma_cov(j, k, i) = NA_REAL;
                  } else {
                    arma_cov(j, k, i) = sumsq_xy / (sqrt(sumsq_x) * sqrt(sumsq_y));
                  }
                  
                } else if (!scale) {
                  arma_cov(j, k, i) = sumsq_xy / (sum_w - sumsq_w / sum_w);
                }
                
              } else {
                arma_cov(j, k, i) = NA_REAL;
              }
              
          } else {
            
            // can be either NA or NaN
            if (std::isnan(x(i, j))) {
              arma_cov(j, k, i) = x(i, j);
            } else {
              arma_cov(j, k, i) = y(i, k);
            }
            
          }
          
        }
        
      }
    }
  }
  
};

// 'Worker' function for computing rolling covariances using a standard algorithm
struct RollCovParallelXX : public Worker {
  
  const RMatrix<double> x;       // source
  const int n;
  const int n_rows_xy;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const bool center;
  const bool scale;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::cube& arma_cov;          // destination (pass by reference)
  
  // initialize with source and destination
  RollCovParallelXX(const NumericMatrix x, const int n,
                    const int n_rows_xy, const int n_cols_x,
                    const int width, const arma::vec arma_weights,
                    const bool center, const bool scale, 
                    const int min_obs, const arma::uvec arma_any_na,
                    const bool na_restore, arma::cube& arma_cov)
    : x(x), n(n),
      n_rows_xy(n_rows_xy), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      center(center), scale(scale),
      min_obs(min_obs), arma_any_na(arma_any_na),
      na_restore(na_restore), arma_cov(arma_cov) { }
  
  // function call operator that iterates by index
  void operator()(std::size_t begin_index, std::size_t end_index) {
    for (std::size_t z = begin_index; z < end_index; z++) {
      
      // from 1D to 3D array (lower triangle)
      int n_unique = n_cols_x * (n_cols_x + 1) / 2;
      int i = z / n_unique;
      int z_unique = z % n_unique;
      int k = n_cols_x -
        floor((sqrt((long double)(4 * n_cols_x * (n_cols_x + 1) - (7 + 8 * z_unique))) - 1) / 2) - 1;
      int j = z_unique - n_cols_x * k + k * (k + 1) / 2;
      
      long double mean_x = 0;
      long double mean_y = 0;
      long double var_x = 0;
      long double var_y = 0;
      
      // don't compute if missing value and 'na_restore' argument is true
      if ((!na_restore) || (na_restore && !std::isnan(x(i, j)) &&
          !std::isnan(x(i, k)))) {
          
          if (center) {
            
            int count = 0;
            long double sum_w = 0;
            long double sum_x = 0;
            long double sum_y = 0;
            
            // number of observations is either the window size or,
            // for partial results, the number of the current row
            while ((width > count) && (i >= count)) {
              
              // don't include if missing value and 'any_na' argument is 1
              // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
              if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j)) &&
                  !std::isnan(x(i - count, k))) {
                  
                  // compute the rolling sum
                  sum_w += arma_weights[n - count - 1];
                sum_x += arma_weights[n - count - 1] * x(i - count, j);
                sum_y += arma_weights[n - count - 1] * x(i - count, k);
                
              }
              
              count += 1;
              
            }
            
            // compute the mean
            mean_x = sum_x / sum_w;
            mean_y = sum_y / sum_w;
            
          }
          
          if (scale) {
            
            int count = 0;
            long double sumsq_x = 0;
            long double sumsq_y = 0;
            
            // number of observations is either the window size or,
            // for partial results, the number of the current row
            while ((width > count) && (i >= count)) {
              
              // don't include if missing value and 'any_na' argument is 1
              // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
              if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j)) &&
                  !std::isnan(x(i - count, k))) {
                  
                  // compute the rolling sum of squares with 'center' argument
                  if (center) {
                    
                    sumsq_x += arma_weights[n - count - 1] *
                      pow(x(i - count, j) - mean_x, (long double)2.0);
                    sumsq_y += arma_weights[n - count - 1] *
                      pow(x(i - count, k) - mean_y, (long double)2.0);
                    
                  } else if (!center) {
                    
                    sumsq_x += arma_weights[n - count - 1] *
                      pow(x(i - count, j), 2.0);
                    sumsq_y += arma_weights[n - count - 1] *
                      pow(x(i - count, k), 2.0);
                    
                  }
                  
              }
              
              count += 1;
              
            }
            
            // compute the unbiased estimate of variance
            var_x = sumsq_x;
            var_y = sumsq_y;
            
          }
          
          int count = 0;
          int n_obs = 0;
          long double sum_w = 0;
          long double sumsq_w = 0;
          long double sumsq_xy = 0;
          
          // number of observations is either the window size or,
          // for partial results, the number of the current row
          while ((width > count) && (i >= count)) {
            
            // don't include if missing value and 'any_na' argument is 1
            // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
            if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j)) &&
                !std::isnan(x(i - count, k))) {
                
                sum_w += arma_weights[n - count - 1];
              sumsq_w += pow(arma_weights[n - count - 1], 2.0);
              
              // compute the rolling sum of squares with 'center' argument
              if (center) {
                sumsq_xy += arma_weights[n - count - 1] * 
                  (x(i - count, j) - mean_x) * (x(i - count, k) - mean_y);
              } else if (!center) {
                sumsq_xy += arma_weights[n - count - 1] * 
                  x(i - count, j) * x(i - count, k);
              }
              
              n_obs += 1;
              
            }
            
            count += 1;
            
          }
          
          // compute the unbiased estimate of covariance
          if ((n_obs > 1) && (n_obs >= min_obs)) {
            
            if (scale) {
              
              // don't compute if the standard deviation is zero
              if ((sqrt(var_x) <= sqrt(arma::datum::eps)) ||
                  (sqrt(var_y) <= sqrt(arma::datum::eps))) {
                arma_cov(j, k, i) = NA_REAL;
              } else {
                arma_cov(j, k, i) = sumsq_xy / (sqrt(var_x) * sqrt(var_y));
              }
              
            } else if (!scale) {
              arma_cov(j, k, i) = sumsq_xy / (sum_w - sumsq_w / sum_w);
            }
            
          } else {
            arma_cov(j, k, i) = NA_REAL;
          }
          
      } else {
        
        // can be either NA or NaN
        if (std::isnan(x(i, j))) {
          arma_cov(j, k, i) = x(i, j);
        } else {
          arma_cov(j, k, i) = x(i, k);
        }
        
      }
      
      // covariance matrix is symmetric
      arma_cov(k, j, i) = arma_cov(j, k, i);
      
    }
  }
  
};

// 'Worker' function for computing rolling covariances using a standard algorithm
struct RollCovParallelXY : public Worker {
  
  const RMatrix<double> x;       // source
  const RMatrix<double> y;       // source
  const int n;
  const int n_rows_xy;
  const int n_cols_x;
  const int n_cols_y;
  const int width;
  const arma::vec arma_weights;
  const bool center;
  const bool scale;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::cube& arma_cov;          // destination (pass by reference)
  
  // initialize with source and destination
  RollCovParallelXY(const NumericMatrix x, const NumericMatrix y,
                    const int n, const int n_rows_xy,
                    const int n_cols_x, const int n_cols_y,
                    const int width, const arma::vec arma_weights,
                    const bool center, const bool scale,
                    const int min_obs, const arma::uvec arma_any_na,
                    const bool na_restore, arma::cube& arma_cov)
    : x(x), y(y),
      n(n), n_rows_xy(n_rows_xy),
      n_cols_x(n_cols_x), n_cols_y(n_cols_y),
      width(width), arma_weights(arma_weights),
      center(center), scale(scale),
      min_obs(min_obs), arma_any_na(arma_any_na),
      na_restore(na_restore), arma_cov(arma_cov) { }
  
  // function call operator that iterates by index
  void operator()(std::size_t begin_index, std::size_t end_index) {
    for (std::size_t z = begin_index; z < end_index; z++) {
      
      // from 1D to 3D array
      int i = z % n_rows_xy;
      int j = z / (n_cols_y * n_rows_xy);
      int k = (z / n_rows_xy) % n_cols_y;
      
      long double mean_x = 0;
      long double mean_y = 0;
      long double var_x = 0;
      long double var_y = 0;
      
      // don't compute if missing value and 'na_restore' argument is true
      if ((!na_restore) || (na_restore && !std::isnan(x(i, j)) &&
          !std::isnan(y(i, k)))) {
          
          if (center) {
            
            int count = 0;
            long double sum_w = 0;
            long double sum_x = 0;
            long double sum_y = 0;
            
            // number of observations is either the window size or,
            // for partial results, the number of the current row
            while ((width > count) && (i >= count)) {
              
              // don't include if missing value and 'any_na' argument is 1
              // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
              if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j)) &&
                  !std::isnan(y(i - count, k))) {
                  
                  // compute the rolling sum
                  sum_w += arma_weights[n - count - 1];
                sum_x += arma_weights[n - count - 1] * x(i - count, j);
                sum_y += arma_weights[n - count - 1] * y(i - count, k);
                
              }
              
              count += 1;
              
            }
            
            // compute the mean
            mean_x = sum_x / sum_w;
            mean_y = sum_y / sum_w;
            
          }
          
          if (scale) {
            
            int count = 0;
            long double sumsq_x = 0;
            long double sumsq_y = 0;
            
            // number of observations is either the window size or,
            // for partial results, the number of the current row
            while ((width > count) && (i >= count)) {
              
              // don't include if missing value and 'any_na' argument is 1
              // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
              if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j)) &&
                  !std::isnan(y(i - count, k))) {
                  
                  // compute the rolling sum of squares with 'center' argument
                  if (center) {
                    
                    sumsq_x += arma_weights[n - count - 1] *
                      pow(x(i - count, j) - mean_x, (long double)2.0);
                    sumsq_y += arma_weights[n - count - 1] *
                      pow(y(i - count, k) - mean_y, (long double)2.0);
                    
                  } else if (!center) {
                    
                    sumsq_x += arma_weights[n - count - 1] *
                      pow(x(i - count, j), 2.0);
                    sumsq_y += arma_weights[n - count - 1] *
                      pow(y(i - count, k), 2.0);
                    
                  }
                  
              }
              
              count += 1;
              
            }
            
            // compute the unbiased estimate of variance
            var_x = sumsq_x;
            var_y = sumsq_y;
            
          }
          
          int count = 0;
          int n_obs = 0;
          long double sum_w = 0;
          long double sumsq_w = 0;
          long double sumsq_xy = 0;
          
          // number of observations is either the window size or,
          // for partial results, the number of the current row
          while ((width > count) && (i >= count)) {
            
            // don't include if missing value and 'any_na' argument is 1
            // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
            if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j)) &&
                !std::isnan(y(i - count, k))) {
                
                sum_w += arma_weights[n - count - 1];
              sumsq_w += pow(arma_weights[n - count - 1], 2.0);
              
              // compute the rolling sum of squares with 'center' argument
              if (center) {
                sumsq_xy += arma_weights[n - count - 1] * 
                  (x(i - count, j) - mean_x) * (y(i - count, k) - mean_y);
              } else if (!center) {
                sumsq_xy += arma_weights[n - count - 1] * 
                  x(i - count, j) * y(i - count, k);
              }
              
              n_obs += 1;
              
            }
            
            count += 1;
            
          }
          
          // compute the unbiased estimate of covariance
          if ((n_obs > 1) && (n_obs >= min_obs)) {
            
            if (scale) {
              
              // don't compute if the standard deviation is zero
              if ((sqrt(var_x) <= sqrt(arma::datum::eps)) ||
                  (sqrt(var_y) <= sqrt(arma::datum::eps))) {
                arma_cov(j, k, i) = NA_REAL;
              } else {
                arma_cov(j, k, i) = sumsq_xy / (sqrt(var_x) * sqrt(var_y));
              }
              
            } else if (!scale) {
              arma_cov(j, k, i) = sumsq_xy / (sum_w - sumsq_w / sum_w);
            }
            
          } else {
            arma_cov(j, k, i) = NA_REAL;
          }
          
      } else {
        
        // can be either NA or NaN
        if (std::isnan(x(i, j))) {
          arma_cov(j, k, i) = x(i, j);
        } else {
          arma_cov(j, k, i) = y(i, k);
        }
        
      }
      
    }
  }
  
};

// 'Worker' function for computing rolling covariances using an online algorithm
struct RollCovOnlineLm : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_xy;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const bool intercept;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::vec& arma_n_obs;        // destination (pass by reference)
  arma::vec& arma_sum_w;
  arma::mat& arma_mean;
  arma::cube& arma_cov;
  
  // initialize with source and destination
  RollCovOnlineLm(const NumericMatrix x, const int n,
                  const int n_rows_xy, const int n_cols_x,
                  const int width, const arma::vec arma_weights,
                  const bool intercept, const int min_obs,
                  const arma::uvec arma_any_na, const bool na_restore,
                  arma::vec& arma_n_obs, arma::vec& arma_sum_w,
                  arma::mat& arma_mean, arma::cube& arma_cov)
    : x(x), n(n),
      n_rows_xy(n_rows_xy), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      intercept(intercept), min_obs(min_obs),
      arma_any_na(arma_any_na), na_restore(na_restore),
      arma_n_obs(arma_n_obs), arma_sum_w(arma_sum_w),
      arma_mean(arma_mean), arma_cov(arma_cov) { }
  
  // function call operator that iterates by column
  void operator()(std::size_t begin_col, std::size_t end_col) {
    for (std::size_t j = begin_col; j < end_col; j++) {
      for (std::size_t k = 0; k <= j; k++) {
        
        int n_obs = 0;
        long double lambda = 0;
        long double w_new = 0;
        long double w_old = 0;      
        long double x_new = 0;
        long double x_old = 0;
        long double y_new = 0;
        long double y_old = 0;
        long double sum_w = 0;
        long double sum_x = 0;
        long double sum_y = 0;
        long double sumsq_w = 0;
        long double sumsq_xy = 0;
        // long double mean_prev_x = 0;
        long double mean_prev_y = 0;
        long double mean_x = 0;
        long double mean_y = 0;
        
        if (width > 1) {
          lambda = arma_weights[n - 2] / arma_weights[n - 1]; // check already passed!
        } else {
          lambda = arma_weights[n - 1];
        }
        
        for (int i = 0; i < n_rows_xy; i++) {
          
          if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(x(i, k))) {
            
            w_new = 0;
            x_new = 0;
            y_new = 0;
            
          } else {
            
            w_new = arma_weights[n - 1];
            x_new = x(i, j);
            y_new = x(i, k);
            
          }
          
          // expanding window
          if (i < width) {
            
            // don't include if missing value and 'any_na' argument is 1
            // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k))) {
              n_obs += 1;
            }
            
            sum_w = lambda * sum_w + w_new;
            sum_x = lambda * sum_x + w_new * x_new;
            sum_y = lambda * sum_y + w_new * y_new;
            sumsq_w = pow(lambda, (long double)2.0) * sumsq_w + pow(w_new, (long double)2.0);
            
            if (intercept && (n_obs > 0)) {
              
              // compute the mean
              // mean_prev_x = mean_x;
              mean_prev_y = mean_y;
              mean_x = sum_x / sum_w;
              mean_y = sum_y / sum_w;
              
            }
            
            // compute the sum of squares
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) && (n_obs > 1)) {
              
              sumsq_xy = lambda * sumsq_xy +
                w_new * (x_new - mean_x) * (y_new - mean_prev_y);
              
            } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(x(i, k))) {
              
              sumsq_xy = lambda * sumsq_xy;
              
            } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) &&
              (n_obs == 1) && !intercept) {
              
              sumsq_xy = w_new * x_new * y_new;
              
            }
            
          }
          
          // rolling window
          if (i >= width) {
            
            // don't include if missing value and 'any_na' argument is 1
            // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) &&
                ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(x(i - width, k)))) {
              
              n_obs += 1;
              
            } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(x(i, k))) &&
              (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j)) && !std::isnan(x(i - width, k))) {
              
              n_obs -= 1;
              
            }
            
            if ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(x(i - width, k))) {
              
              w_old = 0;
              x_old = 0;
              y_old = 0;
              
            } else {
              
              w_old = arma_weights[0];
              x_old = x(i - width, j);
              y_old = x(i - width, k);
              
            }
            
            sum_w = lambda * sum_w + w_new - lambda * w_old;
            sum_x = lambda * sum_x + w_new * x_new - lambda * w_old * x_old;
            sum_y = lambda * sum_y + w_new * y_new - lambda * w_old * y_old;
            sumsq_w = pow(lambda, (long double)2.0) * sumsq_w +
              pow(w_new, (long double)2.0) - pow(lambda * w_old, (long double)2.0);
            
            if (intercept && (n_obs > 0)) {
              
              // compute the mean
              // mean_prev_x = mean_x;
              mean_prev_y = mean_y;
              mean_x = sum_x / sum_w;
              mean_y = sum_y / sum_w;
              
            }
            
            // compute the sum of squares
            if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) &&
                (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j)) && !std::isnan(x(i - width, k))) {
              
              sumsq_xy = lambda * sumsq_xy +
                w_new * (x_new - mean_x) * (y_new - mean_prev_y) -
                lambda * w_old * (x_old - mean_x) * (y_old - mean_prev_y);
              
            } else if ((arma_any_na[i] == 0) && !std::isnan(x(i, j)) && !std::isnan(x(i, k)) &&
              ((arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(x(i - width, k)))) {
              
              sumsq_xy = lambda * sumsq_xy +
                w_new * (x_new - mean_x) * (y_new - mean_prev_y);
              
            } else if (((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(x(i, k))) &&
              (arma_any_na[i - width] == 0) && !std::isnan(x(i - width, j)) && !std::isnan(x(i - width, k))) {
              
              sumsq_xy = lambda * sumsq_xy -
                lambda * w_old * (x_old - mean_x) * (y_old - mean_prev_y);
              
            } else if ((arma_any_na[i] != 0) || std::isnan(x(i, j)) || std::isnan(x(i, k)) ||
              (arma_any_na[i - width] != 0) || std::isnan(x(i - width, j)) || std::isnan(x(i - width, k))) {
              
              sumsq_xy = lambda * sumsq_xy;
              
            }
            
          }
          
          // degrees of freedom and intercept std.error
          if (((int)j == n_cols_x - 1) && ((int)k == n_cols_x - 1)) {
            
            arma_n_obs[i] = n_obs;
            arma_sum_w[i] = sum_w;
            
          }
          
          // intercept
          if (j == k) {
            arma_mean(i, j) = mean_x;
          }
          
          // don't compute if missing value and 'na_restore' argument is true
          if ((!na_restore) || (na_restore && !std::isnan(x(i, j)) &&
              !std::isnan(x(i, k)))) {
              
              // compute the unbiased estimate of variance
              if ((n_obs > 1) && (n_obs >= min_obs)) {
                arma_cov(j, k, i) = sumsq_xy;
              } else {
                arma_cov(j, k, i) = NA_REAL;
              }
              
          } else {
            
            // can be either NA or NaN
            if (std::isnan(x(i, j))) {
              arma_cov(j, k, i) = x(i, j);
            } else {
              arma_cov(j, k, i) = x(i, k);
            }
            
          }
          
          // covariance matrix is symmetric
          arma_cov(k, j, i) = arma_cov(j, k, i);
          
        }
        
      }
    }
  }
  
};

// 'Worker' function for computing rolling covariances using a standard algorithm
struct RollCovParallelLm : public Worker {
  
  const RMatrix<double> x;      // source
  const int n;
  const int n_rows_xy;
  const int n_cols_x;
  const int width;
  const arma::vec arma_weights;
  const bool intercept;
  const int min_obs;
  const arma::uvec arma_any_na;
  const bool na_restore;
  arma::vec& arma_n_obs;        // destination (pass by reference)
  arma::vec& arma_sum_w;
  arma::mat& arma_mean;
  arma::cube& arma_cov;
  
  // initialize with source and destination
  RollCovParallelLm(const NumericMatrix x, const int n,
                    const int n_rows_xy, const int n_cols_x,
                    const int width, const arma::vec arma_weights,
                    const bool intercept, const int min_obs,
                    const arma::uvec arma_any_na, const bool na_restore,
                    arma::vec& arma_n_obs, arma::vec& arma_sum_w,
                    arma::mat& arma_mean, arma::cube& arma_cov)
    : x(x), n(n),
      n_rows_xy(n_rows_xy), n_cols_x(n_cols_x),
      width(width), arma_weights(arma_weights),
      intercept(intercept), min_obs(min_obs),
      arma_any_na(arma_any_na), na_restore(na_restore),
      arma_n_obs(arma_n_obs), arma_sum_w(arma_sum_w),
      arma_mean(arma_mean), arma_cov(arma_cov) { }
  
  // function call operator that iterates by index
  void operator()(std::size_t begin_index, std::size_t end_index) {
    for (std::size_t z = begin_index; z < end_index; z++) {
      
      // from 1D to 3D array (lower triangle)
      int n_unique = n_cols_x * (n_cols_x + 1) / 2;
      int i = z / n_unique;
      int z_unique = z % n_unique;
      int k = n_cols_x -
        floor((sqrt((long double)(4 * n_cols_x * (n_cols_x + 1) - (7 + 8 * z_unique))) - 1) / 2) - 1;
      int j = z_unique - n_cols_x * k + k * (k + 1) / 2;
      
      long double mean_x = 0;
      long double mean_y = 0;
      
      // don't compute if missing value and 'na_restore' argument is true
      if ((!na_restore) || (na_restore && !std::isnan(x(i, j)) &&
          !std::isnan(x(i, k)))) {
          
          if (intercept) {
            
            int count = 0;
            long double sum_w = 0;
            long double sum_x = 0;
            long double sum_y = 0;
            
            // number of observations is either the window size or,
            // for partial results, the number of the current row
            while ((width > count) && (i >= count)) {
              
              // don't include if missing value and 'any_na' argument is 1
              // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
              if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j)) &&
                  !std::isnan(x(i - count, k))) {
                  
                  // compute the rolling sum
                  sum_w += arma_weights[n - count - 1];
                sum_x += arma_weights[n - count - 1] * x(i - count, j);
                sum_y += arma_weights[n - count - 1] * x(i - count, k);
                
              }
              
              count += 1;
              
            }
            
            // compute the mean
            mean_x = sum_x / sum_w;
            mean_y = sum_y / sum_w;
            
          }
          
          int count = 0;
          int n_obs = 0;
          long double sum_w = 0;
          long double sumsq_w = 0;
          long double sumsq_xy = 0;
          
          // number of observations is either the window size or,
          // for partial results, the number of the current row
          while ((width > count) && (i >= count)) {
            
            // don't include if missing value and 'any_na' argument is 1
            // note: 'any_na' is set to 0 if 'complete_obs' argument is FALSE
            if ((arma_any_na[i - count] == 0) && !std::isnan(x(i - count, j)) &&
                !std::isnan(x(i - count, k))) {
                
                sum_w += arma_weights[n - count - 1];
              sumsq_w += pow(arma_weights[n - count - 1], 2.0);
              
              // compute the rolling sum of squares with 'center' argument
              if (intercept) {
                sumsq_xy += arma_weights[n - count - 1] * 
                  (x(i - count, j) - mean_x) * (x(i - count, k) - mean_y);
              } else if (!intercept) {
                sumsq_xy += arma_weights[n - count - 1] * 
                  x(i - count, j) * x(i - count, k);
              }
              
              n_obs += 1;
              
            }
            
            count += 1;
            
          }
          
          // degrees of freedom and intercept std.error
          if ((j == n_cols_x - 1) && (k == n_cols_x - 1)) {
            
            arma_n_obs[i] = n_obs;
            arma_sum_w[i] = sum_w;
            
          }
          
          // intercept
          if (j == k) {
            arma_mean(i, j) = mean_x;
          }
          
          // compute the unbiased estimate of covariance
          if ((n_obs > 1) && (n_obs >= min_obs)) {
            arma_cov(j, k, i) = sumsq_xy;
          } else {
            arma_cov(j, k, i) = NA_REAL;
          }
          
      } else {
        
        // can be either NA or NaN
        if (std::isnan(x(i, j))) {
          arma_cov(j, k, i) = x(i, j);
        } else {
          arma_cov(j, k, i) = x(i, k);
        }
        
      }
      
      // covariance matrix is symmetric
      arma_cov(k, j, i) = arma_cov(j, k, i);
      
    }
  }
  
};

// 'Worker' function for rolling linear models
struct RollLmInterceptTRUE : public Worker {
  
  const arma::cube arma_cov;    // source
  const int n;
  const int n_rows_xy;
  const int n_cols_x;
  const int width;
  const arma::vec arma_n_obs;
  const arma::vec arma_sum_w;
  const arma::mat arma_mean;
  arma::mat& arma_coef;         // destination (pass by reference)
  arma::vec& arma_rsq;
  arma::mat& arma_se;
  
  // initialize with source and destination
  RollLmInterceptTRUE(const arma::cube arma_cov, const int n,
                      const int n_rows_xy, const int n_cols_x,
                      const int width, const arma::vec arma_n_obs,
                      const arma::vec arma_sum_w, const arma::mat arma_mean,
                      arma::mat& arma_coef, arma::vec& arma_rsq,
                      arma::mat& arma_se)
    : arma_cov(arma_cov), n(n),
      n_rows_xy(n_rows_xy), n_cols_x(n_cols_x),
      width(width), arma_n_obs(arma_n_obs),
      arma_sum_w(arma_sum_w), arma_mean(arma_mean),
      arma_coef(arma_coef), arma_rsq(arma_rsq),
      arma_se(arma_se) { }
  
  // function call operator that iterates by slice
  void operator()(std::size_t begin_slice, std::size_t end_slice) {
    for (std::size_t i = begin_slice; i < end_slice; i++) {
      
      arma::mat sigma = arma_cov.slice(i);
      arma::mat A = sigma.submat(0, 0, n_cols_x - 2, n_cols_x - 2);
      arma::mat b = sigma.submat(0, n_cols_x - 1, n_cols_x - 2, n_cols_x - 1);
      arma::vec coef(n_cols_x - 1);
      
      // check if missing value is present
      bool any_na = sigma.has_nan();
      
      // don't compute if missing value 
      if (!any_na) {
        
        // check if solution is found 
        bool status_solve = arma::solve(coef, A, b, arma::solve_opts::no_approx);
        int df_fit = n_cols_x;
        
        // don't find approximate solution for rank deficient system,
        // and the width and current row must be greater than the
        // number of variables
        if (status_solve && (arma_n_obs[i] >= df_fit)) {
          
          // intercept
          arma::mat mean_x = arma_mean.submat(i, 0, i, n_cols_x - 2);
          arma_coef(i, 0) = arma_mean(i, n_cols_x - 1) -
            as_scalar(mean_x * coef);
          
          // coefficients
          arma::mat trans_coef = trans(coef);
          arma_coef.submat(i, 1, i, n_cols_x - 1) = trans_coef;
          
          // r-squared
          long double var_y = sigma(n_cols_x - 1, n_cols_x - 1);
          if ((sqrt(var_y) <= sqrt(arma::datum::eps)) || (var_y < 0)) {
            arma_rsq[i] = NA_REAL;
          } else {
            arma_rsq[i] = as_scalar(trans_coef * A * coef) / var_y;
          }
          
          // check if matrix is singular
          arma::mat A_inv(n_cols_x, n_cols_x);
          bool status_inv = arma::inv(A_inv, A);
          int df_resid = arma_n_obs[i] - n_cols_x;
          
          if (status_inv && (df_resid > 0)) {
            
            // residual variance
            long double var_resid = (1 - arma_rsq[i]) * var_y / df_resid;
            
            // standard errors
            arma_se(i, 0) = sqrt(var_resid * (1 / arma_sum_w[i] +
              as_scalar(mean_x * A_inv * trans(mean_x))));
            arma_se.submat(i, 1, i, n_cols_x - 1) = sqrt(var_resid * trans(diagvec(A_inv)));
            
          } else {
            
            arma::rowvec no_solution(n_cols_x);
            no_solution.fill(NA_REAL);
            
            arma_se.row(i) = no_solution;
            
          }
          
        } else {
          
          arma::rowvec no_solution(n_cols_x);
          no_solution.fill(NA_REAL);
          
          arma_coef.row(i) = no_solution;
          arma_rsq[i] = NA_REAL;
          arma_se.row(i) = no_solution;
          
        }
        
      } else {
        
        arma::rowvec no_solution(n_cols_x);
        no_solution.fill(NA_REAL);
        
        arma_coef.row(i) = no_solution;
        arma_rsq[i] = NA_REAL;
        arma_se.row(i) = no_solution;
        
      }
      
    }
  }
  
};

// 'Worker' function for rolling linear models
struct RollLmInterceptFALSE : public Worker {
  
  const arma::cube arma_cov;    // source
  const int n;
  const int n_rows_xy;
  const int n_cols_x;
  const int width;
  const arma::vec arma_n_obs;
  const arma::vec arma_sum_w;
  arma::mat& arma_coef;         // destination (pass by reference)
  arma::vec& arma_rsq;
  arma::mat& arma_se;
  
  // initialize with source and destination
  RollLmInterceptFALSE(const arma::cube arma_cov, const int n,
                       const int n_rows_xy, const int n_cols_x,
                       const int width, const arma::vec arma_n_obs,
                       const arma::vec arma_sum_w, arma::mat& arma_coef,
                       arma::vec& arma_rsq, arma::mat& arma_se)
    : arma_cov(arma_cov), n(n),
      n_rows_xy(n_rows_xy), n_cols_x(n_cols_x),
      width(width), arma_n_obs(arma_n_obs),
      arma_sum_w(arma_sum_w), arma_coef(arma_coef),
      arma_rsq(arma_rsq), arma_se(arma_se) { }
  
  // function call operator that iterates by slice
  void operator()(std::size_t begin_slice, std::size_t end_slice) {
    for (std::size_t i = begin_slice; i < end_slice; i++) {
      
      arma::mat sigma = arma_cov.slice(i);
      arma::mat A = sigma.submat(0, 0, n_cols_x - 2, n_cols_x - 2);
      arma::mat b = sigma.submat(0, n_cols_x - 1, n_cols_x - 2, n_cols_x - 1);
      arma::vec coef(n_cols_x - 2);
      
      // check if missing value is present
      bool any_na = sigma.has_nan();
      
      // don't compute if missing value 
      if (!any_na) {
        
        // check if solution is found      
        bool status_solve = arma::solve(coef, A, b, arma::solve_opts::no_approx);
        int df_fit = n_cols_x - 1;
        
        // don't find approximate solution for rank deficient system,
        // and the width and current row must be greater than the
        // number of variables
        if (status_solve && (arma_n_obs[i] >= df_fit)) {
          
          // coefficients
          arma::mat trans_coef = trans(coef);
          arma_coef.row(i) = trans_coef;
          
          // r-squared
          long double var_y = sigma(n_cols_x - 1, n_cols_x - 1);
          if ((sqrt(var_y) <= sqrt(arma::datum::eps)) || (var_y < 0)) {
            arma_rsq[i] = NA_REAL;
          } else {
            arma_rsq[i] = as_scalar(trans_coef * A * coef) / var_y;
          }
          
          // check if matrix is singular
          arma::mat A_inv(n_cols_x, n_cols_x);
          bool status_inv = arma::inv(A_inv, A);
          int df_resid = arma_n_obs[i] - n_cols_x + 1;
          
          if (status_inv && (df_resid > 0)) {
            
            // residual variance
            long double var_resid = (1 - arma_rsq[i]) * var_y / df_resid;
            
            // standard errors
            arma_se.row(i) = sqrt(var_resid * trans(diagvec(A_inv)));
            
          } else {
            
            arma::vec no_solution(n_cols_x - 1);
            no_solution.fill(NA_REAL);
            
            arma_se.row(i) = trans(no_solution);
            
          }
          
        } else {
          
          arma::vec no_solution(n_cols_x - 1);
          no_solution.fill(NA_REAL);
          
          arma_coef.row(i) = trans(no_solution);
          arma_rsq[i] = NA_REAL;
          arma_se.row(i) = trans(no_solution);
          
        }
        
      } else {
        
        arma::vec no_solution(n_cols_x - 1);
        no_solution.fill(NA_REAL);
        
        arma_coef.row(i) = trans(no_solution);
        arma_rsq[i] = NA_REAL;
        arma_se.row(i) = trans(no_solution);
        
      }
      
    }
  }
  
};

#endif
