# nix-analyzer

A Nix language server that uses Nix to evaluate expressions.

## Installation

Install using the flake:

```nix
{
    inputs.nix-analyzer.url = github:jm8/nix-analyzer;
    # ...
    home.packages = [
        nix-analyzer.packages.x86_64-linux.default
    ];
}
```

### Vscode
1. Install [jnoortheen.nix-ide](https://marketplace.visualstudio.com/items?itemName=jnoortheen.nix-ide). 
2. Nix: Enable Language Server
3. Set Server Path to nix-analyzer

## Development

This uses a fork of nix with minor changes to the parser and interpreter: [jm8/nix](https://github.com/jm8/nix).

Also uses [kuafuwang/LspCpp](https://github.com/kuafuwang/LspCpp).
