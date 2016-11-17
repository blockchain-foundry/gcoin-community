# Change Log
All notable changes to this project will be documented in this file.

## [1.1.4] - 2016-11-17
### Added
- New RPC: assignfixedaddress, which used to set default address

### Changed
- Update gcoin-compat-openssl.spec

### Removed
- Remove member only policy

### Fixed
- Fix the bug of getlicenselist.


## [1.1.3] - 2016-08-19
### Changed
- Put the data into "main" directory when running gcoind
- Naming refactor : bitcoin -> gcoin

### Removed
- Remove order(exchange) mechanism (ORDER, MATCH, CANCEL).

### Fixed
- RPC refinement : fix rpc message.
- Fix account related functions.
- Apply multi currency to some RPCs

