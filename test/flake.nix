{
  inputs.flake-utils.url = "github:numtide/flake-utils/11707dc2f618dd54ca8739b309ec4fc024de578b";
  outputs = {flake-utils ? null, ...}: flake-utils.lib.;
}
