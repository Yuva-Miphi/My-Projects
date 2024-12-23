#!/bin/bash
#Function to check root
check_root_user() {
  if [ "$EUID" -ne 0 ]; then
    echo "This script must be run as root or with sudo. Exiting."
    exit 1
  fi
}


#Function to check docker is available
check_docker_service() {
  if sudo systemctl status docker >/dev/null 2>&1; then
    echo "Docker service is running."
  else
    echo "Docker service is not running or not installed. Exiting."
    exit 1
  fi
}


#Function to check docker image is available
check_docker_image() {
  IMAGE_NAME="aidaptiv"
  IMAGE_TAG="vNXUN_2_01_00"
  # Check if the image is available in Docker images
  if docker images | awk '{print $1":"$2}' | grep -q "^${IMAGE_NAME}:${IMAGE_TAG}$"; then
    echo "Docker image ${IMAGE_NAME}:${IMAGE_TAG} is available."
  else
    echo "Docker image ${IMAGE_NAME}:${IMAGE_TAG} is not available. Exiting."
    exit 1
  fi
}


# Function to get the container name
get_name() {
  while true; do
    read -p "Enter name for the container: " user_name
    if [ -z "$user_name" ]; then
      echo "Error: Container name is not set.. Please give a container name."
    else
      # Check if the name already exists in the list of Docker containers
      if docker ps -a --format '{{.Names}}' | grep -qw "$user_name"; then
        echo "Error: The container name '$user_name' already exists. Please choose another name."
      else
        export NAME="$user_name"
        break
      fi
    fi
  done
}


# Function to list all NVMe devices
list_nvme_drives() {
  # Get the boot device and its partitions
  BOOT_DEVICE=$(lsblk -no PKNAME $(findmnt -no SOURCE /) 2>/dev/null)
  BOOT_PARTITIONS=$(lsblk -no NAME /dev/$BOOT_DEVICE 2>/dev/null)
  # Find all NVMe devices
  NVME_DEVICES=$(lsblk -d -o NAME -r | grep '^nvme')
  echo "S.No, Device_Path, Mount_Point, Total_Size"
  echo "=========================================="
  count=1
  unique_entries=()
  while IFS= read -r DEVICE; do
    # Skip boot device and its partitions
    if [[ "$DEVICE" == "$BOOT_DEVICE" || "$BOOT_PARTITIONS" =~ "$DEVICE" ]]; then
      continue
    fi
    # Check if the device has partitions
    PARTITIONS=$(lsblk -no NAME /dev/$DEVICE | grep "${DEVICE}p")
    if [ -n "$PARTITIONS" ]; then
      # If partitions exist, only list unique partitions
      PARTITION_ENTRIES=$(lsblk -o NAME,MOUNTPOINT,SIZE -r | grep "^${DEVICE}p" | sort -u)
      while IFS= read -r PART_LINE; do
        # Skip boot partitions
        if [[ "$BOOT_PARTITIONS" =~ $(echo "$PART_LINE" | awk '{print $1}') ]]; then
          continue
        fi    
        PART_INFO=$(echo "$PART_LINE" | awk '{print "/dev/"$1", "$(NF-1)", "$NF}')
        # Check if this entry is already in unique_entries
        if [[ ! " ${unique_entries[@]} " =~ " ${PART_INFO} " ]]; then
          unique_entries+=("$PART_INFO")
          echo "$count. $PART_INFO"
          ((count++))
        fi
      done <<< "$PARTITION_ENTRIES"
    else
      # If no partitions, list the drive
      DRIVE_INFO=$(lsblk -no NAME,MOUNTPOINT,SIZE /dev/$DEVICE | awk '{print "/dev/"$1", "$(NF-1)", "$NF}')   
      # Check if this entry is already in unique_entries
      if [[ ! " ${unique_entries[@]} " =~ " ${DRIVE_INFO} " ]]; then
        unique_entries+=("$DRIVE_INFO")
        echo "$count. $DRIVE_INFO"
        ((count++))
      fi
    fi
  done <<< "$NVME_DEVICES"
}


# Function to select a drive and show its mount point
select_and_show_mount_point() {
  # Call the function to list all NVMe drives
  list_nvme_drives
  echo ""
  declare -A DEVICE_MAP
  count=1
  unique_entries=()
  # Fetch the list of NVMe devices
  NVME_DEVICES=$(lsblk -d -o NAME -r | grep '^nvme')

  while IFS= read -r DEVICE; do
    # Skip boot device and its partitions
    if [[ "$DEVICE" == "$BOOT_DEVICE" || "$BOOT_PARTITIONS" =~ "$DEVICE" ]]; then
      continue
    fi
    # Check if the device has partitions
    PARTITIONS=$(lsblk -no NAME /dev/$DEVICE | grep "${DEVICE}p")
    if [ -n "$PARTITIONS" ]; then
      # If partitions exist, list unique partitions
      PARTITION_ENTRIES=$(lsblk -o NAME,MOUNTPOINT,SIZE -r | grep "^${DEVICE}p" | sort -u)
      while IFS= read -r PART_LINE; do
        if [[ "$BOOT_PARTITIONS" =~ $(echo "$PART_LINE" | awk '{print $1}') ]]; then
          continue
        fi
        PART_INFO=$(echo "$PART_LINE" | awk '{print "/dev/"$1","$(NF-1)","$NF}')
        if [[ ! " ${unique_entries[@]} " =~ " ${PART_INFO} " ]]; then
          unique_entries+=("$PART_INFO")
          DEVICE_MAP[$count]="$PART_INFO"
          ((count++))
        fi
      done <<< "$PARTITION_ENTRIES"
    else
      # If no partitions, list the drive
      DRIVE_INFO=$(lsblk -no NAME,MOUNTPOINT,SIZE /dev/$DEVICE | awk '{print "/dev/"$1","$(NF-1)","$NF}')
      if [[ ! " ${unique_entries[@]} " =~ " ${DRIVE_INFO} " ]]; then
        unique_entries+=("$DRIVE_INFO")
        DEVICE_MAP[$count]="$DRIVE_INFO"
        ((count++))
      fi
    fi
  done <<< "$NVME_DEVICES"

  while true; do
    # Ask user to select a drive by number or press 'e' to exit
    read -p "Enter the S.No. of the NVMe drive: " selected_number
    selected_number=$(echo "$selected_number" | xargs)
    # Validate if the input starts with a number
    if ! [[ "$selected_number" =~ ^[0-9]+$ ]]; then
      echo "Invalid input! Please enter S.No. of the drive..."
      continue
    fi
    # Find the corresponding device from the list
    DEVICE_DETAILS="${DEVICE_MAP[$selected_number]}"
    if [ -n "$DEVICE_DETAILS" ]; then
      IFS=',' read -r DEVICE_PATH MOUNT_POINT SIZE <<< "$DEVICE_DETAILS"
      if [[ "$MOUNT_POINT" == "No Mount Point" ]]; then
        echo "Error: The selected drive doesn't have a mount point. Please select a drive with a mount point."
      else
        echo "Selected device mount point: $MOUNT_POINT"
        break
      fi
    else
      echo "Invalid S.No. Please select a valid drive."
    fi
  done
}




# Function to prompt for paths and store them in an array
get_paths() {
  # Initialize an empty array to store paths
  paths=()
  while true; do
    # Prompt the user to enter a path or 'e' to exit
    read -p "Enter other paths to mount (or 's' to skip): " user_path
    # Check if the user wants to exit
    if [[ "$user_path" == "s" || "$user_path" == "S" ]]; then
      break
    fi
    # If the user entered a path, add it to the array
    paths+=("$user_path")
  done
  # Export the array as a single string with a delimiter (e.g., ',')
  export PATHS=$(IFS=, ; echo "${paths[*]}")
}


generate_docker_run_command() {
  CLEAN_MOUNT_POINT=$(echo "$MOUNT_POINT" | xargs)
  # Start building the docker run command
  docker_cmd="docker run --gpus all -itd --ipc=host --name $NAME --privileged=true --ulimit memlock=-1 --ulimit stack=67108864 -v $CLEAN_MOUNT_POINT:/mnt"
  # Parse the PATHS variable (delimiter ',')
  IFS=',' read -ra path_array <<< "$PATHS"
  for path in "${path_array[@]}"; do
    CLEAN_PATH=$(echo "$path" | xargs)
    docker_cmd="$docker_cmd -v $CLEAN_PATH:$CLEAN_PATH"
  done
  docker_image="aidaptiv:vNXUN_2_01_00"
  docker_cmd="$docker_cmd $docker_image"

  echo "$docker_cmd"
  if $docker_cmd; then
    echo "Container created successfully with name:'$NAME'."
    exit 0
  fi
}



# Main function
main() {
  check_root_user
  check_docker_service
  check_docker_image
  echo "-----Creating Docker Container-----"
  get_name
  echo ""
  select_and_show_mount_point
  echo ""
  get_paths
  echo ""
  generate_docker_run_command
}

# Run the main function
main

