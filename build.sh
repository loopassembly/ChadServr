#!/bin/bash

# Exit on first error
set -e

# Make the script more colorful
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Building ChadServr (Video Chunk Processing Server) ===${NC}"

# Make sure build directory exists
if [ ! -d "build" ]; then
    echo -e "${YELLOW}Creating build directory...${NC}"
    mkdir -p build
fi

# Check for dependencies
echo -e "${YELLOW}Checking for dependencies...${NC}"

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}ERROR: CMake not installed! Please install it first.${NC}"
    echo "  Ubuntu: sudo apt install cmake"
    echo "  MacOS:  brew install cmake"
    echo "  Arch:   sudo pacman -S cmake"
    exit 1
fi

# Check for Boost
if ! ldconfig -p 2>/dev/null | grep -q "libboost_system" && ! ls /usr/local/lib/libboost_system* &> /dev/null; then
    echo -e "${RED}ERROR: Boost libraries not found!${NC}"
    echo "  Ubuntu: sudo apt install libboost-all-dev"
    echo "  MacOS:  brew install boost"
    echo "  Arch:   sudo pacman -S boost"
    exit 1
fi

# Check for FFmpeg
if ! command -v ffmpeg &> /dev/null; then
    echo -e "${RED}ERROR: FFmpeg not found! Video processing requires FFmpeg.${NC}"
    echo "  Ubuntu: sudo apt install ffmpeg"
    echo "  MacOS:  brew install ffmpeg"
    echo "  Arch:   sudo pacman -S ffmpeg"
    exit 1
fi

# Build the project 
cd build
echo -e "${GREEN}Running CMake...${NC}"
cmake .. 

# Detect number of CPU cores for parallel build
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
echo -e "${GREEN}Building with ${CORES} cores...${NC}"
make -j${CORES}

# Handle build result
if [ $? -eq 0 ]; then
    echo -e "\n${GREEN}✓ Build successful!${NC}"
    echo -e "\nTo run the server:"
    echo -e "  ${YELLOW}./build/bin/chadservr${NC}"
    
    # Create runtime directories if needed
    mkdir -p bin/config bin/logs bin/storage/processed bin/storage/temp
    
    # Check & copy config files
    if [ ! -f "bin/config/server_config.json" ]; then
        cp -n ../config/server_config.json bin/config/ 2>/dev/null || true
    fi
    
    echo -e "\nServer will be accessible at: http://localhost:8080"
else
    echo -e "${RED}✗ Build failed${NC}"
    exit 1
fi