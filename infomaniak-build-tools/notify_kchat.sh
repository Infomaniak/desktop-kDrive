#! /bin/bash
#
# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2025 Infomaniak Network SA
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# Check if the environment variable KCHAT_WEBHOOK_URL is set
if [ -z "$KCHAT_WEBHOOK_URL" ]; then
    echo "Error: The environment variable KCHAT_WEBHOOK_URL is not set."
    exit 1
fi

# Check if a message argument is provided
if [ $# -lt 1 ]; then
    echo "Usage: $0 '<message>'"
    exit 1
fi

# Get the message
MESSAGE="$1"

# Format the JSON payload
PAYLOAD=$(jq -n --arg text "$MESSAGE" '{text: $text}')

# Send the POST request using curl
RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" -X POST \
    -H "Content-Type: application/json" \
    -d "$PAYLOAD" \
    "$KCHAT_WEBHOOK_URL")

# Check the HTTP response code
if [ "$RESPONSE" -eq 200 ] || [ "$RESPONSE" -eq 204 ]; then
    echo "Webhook sent successfully."
else
    echo "Error sending webhook. HTTP Code: $RESPONSE"
    exit 1
fi
