#!/bin/bash

set -e # Exit on error

# get url of current origin
OLD_ORIGIN=$(git config --get remote.origin.url)

if [ -z "$OLD_ORIGIN" ]; then
  echo "Failed to get the remote origin URL. Please setup SSH Keys first!"
  exit 1  # Exit with a non-zero status
else
  echo "Success. Remote origin URL: $OLD_ORIGIN"
fi

# set remote url to template repo
git remote set-url origin git@github.com:OSU-OS2-S23/JOS-Labs.git

# fetch all branches (include commit history)
git fetch

# set remote back to old origin
git remote set-url origin $OLD_ORIGIN

# set push default to simple
git config --global push.default simple

# individually checkout to each branch and push
git checkout master
git push


for branch in lab4 lab3 lab2 lab1
do
  git checkout $branch
  git push
done

echo "End of the script."
