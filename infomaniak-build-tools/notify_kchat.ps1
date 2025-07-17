<#
 Infomaniak kDrive - Desktop App
 Copyright (C) 2023-2024 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
#>

# Check if the environment variable KCHAT_WEBHOOK_URL is set
if (-not $env:KCHAT_WEBHOOK_URL) {
    Write-Host "Error: The environment variable KCHAT_WEBHOOK_URL is not set."
    exit 1
}

# Check if a message argument is provided
if ($args.Count -lt 1) {
    Write-Host "Usage: .\notify_kchat.ps1 '<message>'"
    exit 1
}

# Get the message
$message = $args[0]

# Format the JSON payload
$payload = @{ text = $message } | ConvertTo-Json -Compress

# Send the POST request using Invoke-RestMethod
try {
    $response = Invoke-RestMethod -Uri $env:KCHAT_WEBHOOK_URL -Method Post -Body $payload -ContentType "application/json"
    Write-Host "Webhook sent successfully."
} catch {
    Write-Host "Error sending webhook: $_"
    exit 1
}

