#include <Rcpp.h>
#include <iostream>
using namespace Rcpp;
using namespace std;

// [[Rcpp::export]]
NumericVector smthSpatTmp(NumericMatrix x, double d, double t)
{
  int N = x.nrow ();
  NumericVector s (N);

  int pct = 0;
  std::cout << "0%";
  for (int i=0; i<N; i++)
  {
	int npct = (i + 1) * 100 / N;
	if (npct > pct)
	{
	  pct = npct;
	  std::cout << "\r" << pct << "%";
	  std::cout.flush ();
    }
	// if (is_na (x[i,1])) s[i] = NA_REAL;
	double si = 0;
	int M = 0;
	for (int j=0; j<N; j++)
	{
	  double dij = sqrt(pow(x(i,1) - x(j,1), 2) + pow(x(i,2) - x(j,2), 2));
	  if (//!is_na (x[j,1]) &&
		  dij < d & x(j,3) > x(i,3) - t && x(j,3) < x(i,3) + t)
	  {
		si += x(j,0);
		M++;
	  }
	}
	s[i] = si / M;
  }
  std::cout << "\n";
  return s;
}
