data {
	int N;
	real lat[N];
	real lng[N];
	int t[N];
	
	int Q;
	real sx[Q];
	real sy[Q];
	real sdist[Q];

    real pi;

    int Nt;
}

parameters {
    real<lower=0,upper=30> v[Nt-1];
}

transformed parameters {
    real d[Nt];
    
    real X[N];
    real Y[N];
    
    d[1] = 0;
    for (i in 2:Nt)
        d[i] = d[i-1] + v[i-1];
    
    for (i in 1:N) {
        int j = 1;
        while (d[t[i]] > sdist[j] && j < Q) j = j + 1;
        // calculate the coordinates ...
        X[i] = sx[j];
        Y[i] = sy[j];
    }
}

model {
    for (i in 1:(Nt - 1))
        v[i] ~ uniform(0, 30);

    // calculate log density
    for (i in 1:N) {
        real d2 = pow(X[i] - lng[i], 2) +
                 pow(Y[i] - lat[i], 2);
        target += - log(2 * pi * 5.0) -
            d2 / (2 * pow(5.0, 2));
    }
}

