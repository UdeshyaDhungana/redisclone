# require argument

if [ -z "$1" ]; then
	echo "Enter commit message"
	exit 1
else
	# copy
	rsync -ravPz app ~/Desktop/codecrafters-redis-c

	# commit and push
	cd ~/Desktop/codecrafters-redis-c
	git add .
	git commit --allow-empty -m "$1"
	git push origin master
fi


