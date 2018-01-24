## list files from --args DATE and cycle through them

ca <- commandArgs(TRUE)
## use yesterday as the default
date <- if (length(ca) == 0) Sys.Date() - 1 else as.Date(ca)


getFiles <- function(date) {
	ds <- as.character(date)
	cat("Retrieving list of files for", ds, "...")
	dir <- gsub("-", "/", ds)
	datef <- file.path('/mnt', 'storage', 'history', gsub("-", "", dir))
	cmd <- sprintf("ssh tom@130.216.51.230 find %s -type f -name '*.pb'", datef)
	files <- system(cmd, intern = TRUE)
	cat(" done.\n")

	times <- as.factor(sapply(files, function(f) { #strsplit(files, ":"), function(f) {
		x <- strsplit(as.character(as.POSIXct(strsplit(f, "_|[.]")[[1]][3], format="%Y%m%d%H%M%S")), ":")[[1]]
		x[3] = 30 * (as.numeric(x[3]) %/% 30)
		x = as.POSIXct(paste(x, collapse = ":"))
		format(x, "%H:%M:%S")
	}))
	file.list <- tapply(files, times, c)

	cat("Files organised, beginning realtime simulation.\n")

	i <- 1
	N <- length(file.list)
	for (i in seq_along(file.list)) {
		if (i >= 5010) break;
		fs <- file.list[[i]]
		cat(sprintf("\rLoaded file %s of %s", i, N))
		whichvp <- grep("vehicle_locations", fs)
		whichtu <- grep("trip_updates", fs)
		if (length(whichvp) == 1) {
			cmd <- sprintf("scp -q tom@130.216.51.230:%s build/.vp.pb", fs[whichvp])
			system(cmd)
		}
		if (length(whichtu) == 1) {
			cmd <- sprintf("scp -q tom@130.216.51.230:%s build/.tu.pb", fs[whichtu])
			system(cmd)
		}
		if (length(whichvp) == 1) system("mv build/.vp.pb build/vp.pb")
		if (length(whichtu) == 1) system("mv build/.tu.pb build/tu.pb")
		while (file.exists("build/vp.pb") || file.exists("build/tu.pb")) {
			Sys.sleep(1)
		}
	}

	cat("\nDone!")
}


getFiles(date)
