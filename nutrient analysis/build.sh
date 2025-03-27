# Update Local Repository
git stash              # Stash any uncommitted changes to avoid merge conflicts
git pull --rebase      # Pull latest changes with rebase to keep history clean
git stash pop          # Restore stashed changes

# Define Current Working Directory
dir1=${PWD}            # Store the current directory path in `dir1`

# Remove Previous Build
rm -rf _site           # Deletes the `_site` directory to ensure a clean build

# Check if Quarto is Installed
if ! command -v quarto &> /dev/null; then
    echo "Error: Quarto is not installed. Please install it and try again."
    exit 1
fi

# Build the Website with Quarto
quarto render 2>&1 | tee build.log  # Generates the website and logs output

# Clean Up Unnecessary Files
find _site -name ".DS_Store" -delete  # Removes all `.DS_Store` files

# Set Correct Permissions for All Files and Directories
find _site -type f -exec chmod 644 {} +  # Set files to `644` (readable by all, writable by owner)
find _site -type d -exec chmod 755 {} +  # Set directories to `755` (readable and executable by all)

# Ask User If They Want to Push Changes to GitHub
read -r -p 'Would you like to push to GITHUB? (y/n)? ' answer
if [[ "$answer" =~ ^[Yy]$ ]]; then 

    git config http.postBuffer 20242880000  # Increases buffer size to handle large files

    # Pull Latest Changes Before Committing
    git pull --rebase  

    # Get Commit Message from User
    read -r -p 'ENTER COMMIT MESSAGE: ' message
    echo "Commit message: $message" 

    # Commit and Push to GitHub
    git add .
    git commit -m "$message"
    git push

else
    echo "NOT PUSHING TO GITHUB!"
fi
