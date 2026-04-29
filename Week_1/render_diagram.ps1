<#
Render the Mermaid diagram to PNG using mermaid-cli.

Requirements:
- Node.js and npm installed.

Usage (one-time install globally):
    npm install -g @mermaid-js/mermaid-cli
    mmdc -i diagram.mmd -o diagram.png

Or (without installing globally) using npx:
    npx @mermaid-js/mermaid-cli -i diagram.mmd -o diagram.png

This script runs the npx command so you can just:
    .\render_diagram.ps1
#>

# Path to input and output
$input = "diagram.mmd"
$output = "diagram.png"

# Run npx mermaid-cli to render PNG
Write-Host "Rendering $input -> $output (requires Node/npm)"

$cmd = "npx @mermaid-js/mermaid-cli -i `"$input`" -o `"$output`""
Write-Host "Running: $cmd"

Invoke-Expression $cmd

if (Test-Path $output) {
    Write-Host "Rendered: $output"
} else {
    Write-Host "Rendering failed. Ensure Node.js is installed and try the command above." -ForegroundColor Red
}
