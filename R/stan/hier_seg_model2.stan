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
  real<lower=0,upper=100> intercept[L]; // the intercept
  real beta[K];                         // The spline coefficients
  vector<lower=0,upper=24>[2] tau;      // Middle of peak time
  vector<lower=0.5,upper=2>[2] omega;   // The width of peak time
  real mu_alpha[L,2];                   // The effect of peak hour on speed - mean

  real<lower=0> sigma;                  // variance of observed speeds
}
transformed parameters {
  real eta[N];
  real alpha[L,2];
  vector[2] z;

  for (i in 1:L) {
    for (j in 1:2) {
      alpha[i,j] = (mu_alpha[i,j] < 0) ? 0 : mu_alpha[i,j];
    }
  }

  for (i in 1:N) {
    for (j in 1:2) {
      z[j] = alpha[s[i],j] * exp(-pow(t[i] - tau[j], 2) / 2 * pow(omega[j], 2));
    }
    eta[i] = intercept[s[i]] - sum(z);
  }

  for (m in 1:M) {
    eta[Bij[m,1]] += beta[Bij[m,2]] * Bval[m];
  }
}
model {
  // The basic spline for each segment (speed ~ distance)
  beta ~ normal(0, 1e6);
  intercept ~ uniform(0, 100);

  // The peak-hour effect (speed ~ time)
  for (i in 1:L)
    for (j in 1:2)
      mu_alpha[i,j] ~ normal(0, 10);

  // The actual time of peak-hour
  tau[1] ~ normal(8, 1);
  tau[2] ~ normal(17, 1);
  omega ~ uniform(0.5, 2);

  // The likelihood
  sigma ~ cauchy(0, pow(2.5, -2));
  y ~ normal(eta, pow(sigma, 0.5));
}
generated quantities {
  
}
