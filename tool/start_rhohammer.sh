#!/bin/bash

CONFIG_FILE="config.json"

# Initialize configuration variables
init_config() {
    # Check if jq command is available
    if ! command -v jq &> /dev/null; then
        echo "Error: jq command is required"
        echo "Ubuntu/Debian: sudo apt-get install jq"
        echo "CentOS/RHEL: sudo yum install jq"
        return 1
    fi

    # Check if config file exists
    if [ ! -f "$CONFIG_FILE" ]; then
        echo "Error: Config file $CONFIG_FILE does not exist"
        return 1
    fi

    # Read parameters from JSON
    RANK=$(jq -r '.RANK // 2' "$CONFIG_FILE")
    BANKGROUP=$(jq -r '.BANKGROUP // 4' "$CONFIG_FILE")
    BANK=$(jq -r '.BANK // 4' "$CONFIG_FILE")
    SAMSUNG=$(jq -r '.SAMSUNG // 0' "$CONFIG_FILE")
    CONFIG_DIMM=$(jq -r '.DIMM // 1' "$CONFIG_FILE")
    
    return 0
}

# Generate command function
generate_rhoHammer_cmd() {
    local DIMM_ID=${1:-1}
    local RUNTIME_LIMIT=${2:-120}
    
    # Initialize configuration
    if ! init_config; then
        return 1
    fi

    # Use DIMM value from config file (command line parameter takes priority)
    if [ -z "$1" ]; then
        DIMM_ID=$CONFIG_DIMM
    fi

    # Generate geometry string
    GEOMETRY="${RANK},${BANKGROUP},${BANK}"

    # Build command
    CMD="sudo ./rhoHammer --dimm-id $DIMM_ID --runtime-limit $RUNTIME_LIMIT --geometry $GEOMETRY"

    # Add --samsung parameter based on config
    if [ "$SAMSUNG" = "1" ]; then
        CMD="$CMD --samsung"
    fi

    # Add fixed parameters
    CMD="$CMD --sweeping"
    
    # Return command
    echo "$CMD"
}

# Execute command function
execute_rhoHammer() {
    local DIMM_ID=${1:-1}
    local RUNTIME_LIMIT=${2:-120}
    
    CMD=$(generate_rhoHammer_cmd "$DIMM_ID" "$RUNTIME_LIMIT")
    if [ $? -ne 0 ]; then
        return 1
    fi
    
    # Display generated information
    echo "=== Command Parameter Information ==="
    echo "Config file: $CONFIG_FILE"
    echo "DIMM ID: $DIMM_ID"
    echo "Runtime: $RUNTIME_LIMIT seconds"
    echo "Geometry: $GEOMETRY (RANK=$RANK, BANKGROUP=$BANKGROUP, BANK=$BANK)"
    echo "Samsung: $SAMSUNG"
    echo "Generated command:"
    echo "$CMD"
    echo "====================================="

    # Execute command
    echo "Starting command execution..."
    eval $CMD
}