# Change Log
All notable changes to this project will be documented in this file.

## [1.2.1.1] - 2017-06-15
### Fixed
- Fix incorrect date format in the spec file.

## [1.2.1] - 2017-06-12
### Changed
- Reorder type of transaction.
- Prevent same pubkey in a alliance redeem script.

### Fixed
- Fix voting bug when -txindex turn on.
- Fix rpc signrawtransaction for license/miner related tx.
- Fix assignfixedaddress.
- Fix CheckTxFeeAndColor.
- Fix bug when transferring license.


## [1.2] - 2017-02-18
### Added
- A new role in system : Miner.
- A new Consensus address which is a multi-sig address merge by all alliance to handler alliance issue.
- rpc addalliance.
- rpc setalliance.
- rpc addminer.
- rpc revokeminer.
- rpc mintforlicense.
- rpc mintforminer.

### Changed
- Separate the permission of mining and other functions that should be owned by alliance.
- Use multi-sig policy to handle concensus of all alliance (like voting, issuing license, add/revoke miner).
- Allow higher fee.
- Update COPYING.
- Increase limitation of multi-sig address (at least 100-of-100 now).

### Removed
- BANVOTE type transaction and relative fuction/rpcs.
- rpc sendvotetoaddress.


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

