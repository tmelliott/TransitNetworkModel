## list files from --args DATE and cycle through them

ca <- commandArgs(TRUE)
## use yesterday as the default
date <- if (length(ca) == 0) Sys.Date() - 1 else as.Date(ca)


getFiles <- function(date) {
	datef <- gsub("-", "", date)
	cmd <- sprintf("ssh tom@130.216.51.230 find /mnt/storage/historical_data -type f -name 'vehicle_locations_%s*.pb'", datef)
	files <- system(cmd, intern = TRUE)

	files <- sort(files)

	i <- 1
	N <- length(files)
	while (length(files) > 0) {
		file <- files[1]
		files <- files[-1]
		cat(sprintf("\rLoaded file %s of %s", i, N))
		i <- i+1
		cmd <- sprintf("scp -q tom@130.216.51.230:%s build/vp.pb", file)
		system(cmd)
		while (file.exists("build/vp.pb")) {
			Sys.sleep(1)
		}
	}

	cat("\n Done!")
}


getFiles(date)
