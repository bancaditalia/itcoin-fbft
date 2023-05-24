# Benchmark framework

This folder contains the code and the configurations to repeat the benchmark experiments.

## Preliminaries
### Python environment

- Install [Poetry](https://python-poetry.org/)
- Set up the virtual environment specified by the `pyproject.toml`:

```
poetry shell
poetry install
```

### ItCoin Core

Make sure you have the ItCoin project cloned in directory `../../itcoin-core`
(i.e. at the same level of `itcoin-pbft`).

### Run the benchmark framework locally

```
poetry shell
fab local
```

### Debugging the benchmark framework with VSCode

Get the fab path and python path of the virtual env, using the which command.

```
which fab
which python
```

In the launch configuration:

* in "program" use the fab path of the virtual env
* in "python" use python path of the virtual env
* in "args" specify the fabric task you want to run. The list of possible tasks can be seen running the command `fab --list`

```
  "configurations": [
      {
        "name": "Python: Benchmarking framework",
        "type": "python",
        "request": "launch",
        "python": "/home/ubuntu/.local/share/virtualenvs/benchmark-hsk8VOKG/bin/python",
        "program": "/home/azureuser/.local/share/virtualenvs/benchmark-hsk8VOKG/bin/fab",
        "args": ["local"],
        "cwd": "/home/azureuser/itcoin/itcoin-pbft/benchmark",
        "console": "integratedTerminal"
    },
```

### Run the benchmark framework on AWS
The file [settings.json](settings.json) (located in itcoin-pbft/benchmark) contains all the configuration parameters of the testbed to deploy.

The command

```
fab create
```

will create by default 1 (validator) EC2 instance and 1 (client) EC2 instance per every region specified in the [settings.json](settings.json). If you need to create more than one EC2 instance per region just pass the number of instances as parameter, e.g:

```
fab create --nodes=<n> --clients=<c>
```

When created, the EC2 instances will be already running.
The EC2 instances can be started, stopped and terminated using the commands:

```
fab start
```

```
fab stop
````

```
fab destroy
```

The first time EC2 instances are created we need to install the itcoin-pbft code base and its dependencies, using the command:

```
fab install <github_token>
```

The param `github_token` is one of your personal tokens with the access rights to read (and then clone) the repo itcoin-pbft and itcoin-core.


Example of usage of `fab local`:
```
fab local config-examples/experiment-1.json output-directory
```

Example of usage of `fab remote`:
```
fab remote config-examples/experiment-2.json output-directory
```

Example of usage of `fab generate` (output in `configurations/`):
```
fab generate configurations
```

Example of usage of `fab run-all CONFIG_DIR IS_LOCAL OUTPUT_DIR PLOTS_DIR`:
```
fab run-all configurations True all-results all-plots
```


## Setup Signet network

In this section we set up and run a signet network with the scripts `setup.signet.py` and `run-signet.py`.

The tool `setup-signet.py` creates data directories for a set of Signet nodes.

Usage:
```
Usage: setup-signet.py [OPTIONS]

  Set up a Signet network configuration directory.

Options:
  --output-directory DIRECTORY   Path to output directory.  [required]
  --itcoin-core-dir DIRECTORY    Path to Itcoin Core project.  [required]
  --nb-validators INTEGER RANGE  Number of validators in the network.  [x>=1;
                                 required]
  --help                         Show this message and exit.
```

Example:
```
./setup-signet.py --output-directory output --itcoin-core-dir ../../itcoin-core --nb-validators 4
```


## Run Signet network

The script `run-signet.py` can be used to run a Signet network (configured with `setup-signet.py`).

Usage:
```
Usage: run-signet.py [OPTIONS]

  Run a network of signet nodes.

Options:
  --itcoin-core-dir DIRECTORY  Path to Itcoin Core project.  [required]
  --signet-dir DIRECTORY       Path to signet directory.  [required]
  --help                       Show this message and exit.
```

Example:
```
./run-signet.py --signet-dir output --itcoin-core-dir ../../itcoin-core
```

To test RPC endpoints:
```
cd output
bitcoin-core.cli -datadir=node00 getblockchaininfo
```
