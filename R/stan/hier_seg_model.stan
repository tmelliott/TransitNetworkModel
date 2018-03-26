data {
  int N;                                // number of observations
  int M;                                // number of rows in B
  int K;                                // number of betas
  int L;                                // number of segments

  int Bij[M,2];                         // the basis (sparse-) matrix
  real Bval[M];                         // part 2 of the basis matrix
  vector[N] y;                          // observed speeds
  vector[N] t;                          // time
  int s[N];                             // segment
  int sk[K];                            // which segment each beta belongs to
}
parameters {
  real beta[K];                         // The spline coefficients
  vector<lower=0,upper=1>[L] pi1;       // P[segment is effected by morning peak hour]
  vector<lower=0,upper=1>[L] pi2;       // P[segment is effected by evening peak hour]
  vector<lower=0,upper=24>[2] tau;      // Middle of peak time
  vector<lower=0.5,upper=2>[2] omega;   // The width of peak time
  vector<lower=0,upper=1>[2] alpha;     // The multiplicative effect of peak hour on speed

  real<lower=0> sigma;                  // variance of observed speeds
}
transformed parameters {
  real eta[N];
  for (i in 1:N) {
    eta[i] = 0;
  }
  for (m in 1:M) {
    eta[Bij[m,1]] += beta[Bij[m,2]] * Bval[m];   
  }
}
model {
  // The basic spline for each segment (speed ~ distance)
  beta ~ normal(0, 1e6);

  // The peak-hour effect (speed ~ time)
  pi1 ~ beta(0.5, 0.5);
  pi2 ~ beta(0.5, 0.5);

  // The actual time of peak-hour
  tau[1] ~ normal(8, 1);
  tau[2] ~ normal(17, 1);
  omega ~ uniform(0.5, 2);
  alpha ~ beta(1, 1);

  // The likelihood
  sigma ~ cauchy(0, pow(2.5, -2));
  y ~ normal(eta, pow(sigma, 0.5));
}
generated quantities {
  
}
