data {
    int<lower=0> N;
    int<lower=0> x[N];
    real<lower=0> y[N];
    int<lower=0,upper=23> h[N];

    int<lower=0> M;
    int<lower=0,upper=23> H[M];
}
parameters {
    real<lower=0,upper=60> mu;
    real<lower=0> beta[M];
    real<lower=0> sigmasq;
    // real<lower=0> tausq;
    real<lower=0> tau;
}
transformed parameters {
    real alpha[M];
    real<lower=0> sigma;
    // real<lower=0> tau;

	// alpha[1] = 0;
    for (j in 1:M)
        alpha[j] = beta[j];
    sigma = sqrt(sigmasq);
    // tau = sqrt(tausq);
}
model {
    for (j in 1:M)
        beta[j] ~ exponential(tau);
    tau ~ exponential(1);
    sigmasq ~ uniform(0, 100);
    for (i in 1:N)
        y[i] ~ normal(mu - alpha[h[i]], sigma);
}
generated quantities {
    real yhat[M];
    for (j in 1:M)
        yhat[j] = mu - alpha[j];
}
