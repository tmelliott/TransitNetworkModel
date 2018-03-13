set.seed(359)
set.seed(373)
s <- numeric(100)
s[1] <- 0
for (i in 2:length(s)) {
    s[i] <- truncnorm::rtruncnorm(1, 0, 30, s[i-1], 2)
}
d <- cumsum(s)
t <- c(1, sort(round(runif(10, 0, 100))), 100)
##z <- c(0, sort(round(runif(10, 0, max(d)))))
z <- rnorm(length(t), d[t], 3)
z[1] <- 0
plot(t, z, xlim = c(0, 100), ylim = c(0, max(d)))
lines(d)

plot(z, t, ylim = c(0, 100), xlim = c(0, max(d)))
lines(d, 1:100)
S = c(133, 308)

Vmin <- 1 / 30


library(splines)

fit <- lm(t ~ bs(z, knots = S, degree = 2))
summary(fit)

lines(1:max(z), predict(fit, newdata = data.frame(z = 1:max(z))),
      lty = 2)
#lines(predict(fit, newdata = data.frame(z = 1:max(z))), 1:max(z),
#      lty = 2)



