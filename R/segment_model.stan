data {
    int<lower=0> N;
    int<lower=0> x[N];
    real<lower=0> y[N];
    int<lower=0,upper=23> h[N];

    int<lower=0> M;
    int<lower=0,upper=23> H[M];

    // int Vmax;

	// int<lower=0> S;
	// int<lower=0> speed[S];
	// vector<lower=0,upper=1>[S] pspeed;
}
parameters {
    real<lower=0,upper=100> mu;
    real<lower=0> beta[M];
    real<lower=0> sigmasq;
    real<lower=0> tau;
    real<lower=5,upper=11> Vmax;
}
transformed parameters {
    real alpha[M];
    real<lower=0> sigma;

	alpha[1] = 0;
    for (j in 2:M)
        alpha[j] = beta[j];
    sigma = sqrt(sigmasq);
}
model {
	// pi ~ dirichlet(pspeed);
    Vmax ~ normal(5, 1);
	mu ~ uniform(0, floor(Vmax) * 10 - 20);
    for (j in 1:M)
        beta[j] ~ normal(0, 1);
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
