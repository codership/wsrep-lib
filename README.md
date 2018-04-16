# Introduction

Project name: Trrep - Transaction Replication

The purpose of this project is to implement C++ wrapper
for wsrep API with additional convenience for transaction
processing.

Integration of wsrep API to mysql-wsrep has shown that even
though wsrep API is supposed to provide self contained API
for transaction replication, also DBMS side integration requires
lots of inter thread coordination and state book keeping due to BF
aborts caused by high priority transactions. Also transaction states
are not exposed by the wsrep API, which essentially requires the
DBMS integration to duplicate the state tracking independently of
the wsrep provider.

This project will abstract away most of the transaction state
keeping required on DBMS side and will provide simple
C++ interface for key set population, data set population,
(2PC) commit processing, write set applying etc.

This project will also provide unit testing capability for
DBMS integration and wsrep provider implementation.


The rest of the document dscribes the proposed API in more detail
and should be replaced with automatically generated documentation
later on (doxygen).

# High Level Design

## Provider

Context for wsrep provider which will encapsulate provider loading,
configuration, registering callbacks.

The DBMS implementation will provide an instance of provider
observer class which is called appropriately when the wsrep provider
calls the registered callbacks.



## Server Context

Server attributes:
* Identifiers (name, uuid)

Server capabilities:
* Rollback mode (async or sync)
*

Abstract Methods:
* on_connect()
* on_view()
* on_sync()
* on_apply()
* on_commit()


### Rollback mode

High priority transactions may induce BF aborts due to lock conflicts
during HPT applying. The locally running transaction must be brute force
aborted (BF abort) if the local transaction holds a lock which
HPT is trying to lock and the local transaction is either not ordered
or is ordered after HPT. If HTP can just steal the particular lock
and mark the LT as BF abort victim, the rollback is said to be asynchronous;
client thread will process the rollback once it gains the control again.
On the other hand, if the rollback mode is synchronous, the HPT
must wait until the LT has been rolled back and locks have been
released before it can continue applying. In such a case a background
rollbacker thread may be used to run the rollback process if the
client thread is currently idle, i.e. the control is in the client
program which drives the transaction.

## Client Context

Attributes
* Identifier
* Mode (local, applier)
* State (idle, exec, quit)

Abstract methods:
* before_command() - executed before a command from client is processed
* after_command() - executed after a command from client has been processed
* before_statement() - executed before a statement from client is processed
* after_statement - executed after a statement from a client has been processed

The following subsections briefly describe what would be the
purpose of the abstract methods in mysql-wsrep implementattion.
In other DBMS systems the purpose may vary.

### Before_command()

If the server supports only synchronous rollback, this method must make
sure that any rollback rollback processing is finished before the control
is given back to the client thread.

### After_command()

Bookkeeping.

### Before_statement()

Checks which are run before the actual parsed statement is executed.
These may include:
* Check if dirty reads are allowed
* ...

### After_statement()

Checks if the statement can and should be retried.

## Transaction Context

Attributes
* Client context - pointer or reference to client context
* Identifier - an identifier which uniquely identifies the transaction
  on the server the client driven execution happens

## Local Transaction Replication

See https://github.com/codership/Engineering/wiki/Transaction_coordinator_design

Attributes:
* Identifier (interger, uuid or similar)
* State (executing, certifying, must_abort, etc)

Methods:
* append_key()
* append_data()
* after_row()
* before_prepare()
* after_prepare()

### Append Key

Appends a certification key into transaction key set buffer.

### Append Data

Append a write set data fragment into transaction write set
buffer.

### After Row

Streaming replication specific.

### Before_prepare()

This method is executed before the prepare phase is run for DBMS
engines.

### After_prepare()

This method is executed after the prepare phase is run for DBMS engines.
The populated write set is replicated and certified at this stage.
If the streaming replication is enabled, the final write set may
only contain an empty commit fragment which triggers commit
on remote servers.

## Remote Transaction Applying

Methods:
* apply()

## Commit Order Coordinator

Commit order coordinator is a common for both local and remote
transaction. Currently this is implemented in transaction_context
class, check out later if it make sense to refactor commit ordering
into separate class.

Methods:

* before_commit()
* ordered_commit()
* after_commit()



