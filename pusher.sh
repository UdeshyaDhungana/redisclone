set -e

if [ "$(hostname)" == "Udeshyas-MacBook-Air.local" ]; then
    echo "In Udeshyas-MacBook-Air.local. Running script...";
    RSYNC_PATH="../../codecrafters-redis-c/"
    CD_PATH="../../codecrafters-redis-c/"
elif [ "$(hostname)" == "NP-UDH-MBP-02.local" ]; then
    echo "In NP-UDH-MBP-02.local. Running script...";
    RSYNC_PATH="../codecrafters-redis-c/"
    CD_PATH="../codecrafters-redis-c/"
else
    echo "hostname does not match.";
    exit 1;
fi

if [ -z "$1" ]; then
    echo "Enter commit message"
    exit 1
else
    # copy
    rsync -ravPz app "$RSYNC_PATH"

    # commit and push
    cd "$CD_PATH"
    git add .
    git commit --allow-empty -m "$1"
    git push origin master
fi
