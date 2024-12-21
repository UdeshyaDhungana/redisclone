# you won't need this; this is for syncing with codecrafter's repo
set -e

if [ "$(hostname)" == "Udeshyas-MacBook-Air.local" ] || [ "$(hostname)" == "NP-UDH-MBP-02.local" ]; then
    echo "In the host device. Running script...";
else
    echo "hostname does not match.";
    exit 1;
fi


if [ -z "$1" ]; then
	echo "Enter commit message"
	exit 1
else
	# copy
	rsync -ravPz app ../codecrafters-redis-c/

	# commit and push
	cd ../codecrafters-redis-c/
	git add .
	git commit --allow-empty -m "$1"
	git push origin master
fi
