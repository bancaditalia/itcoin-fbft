# FBFT benchmark framework

This folder contains the code and the configurations to perform benchmark experiments on FBFT.

## Preliminaries

- Install benchmark dependencies with apt (system-wide):
```
$ sudo apt install --no-install-recommends -y tmux
```

- Install the Poetry python dependency manager following the official [installation guide](https://python-poetry.org/docs/#installation)
- Set up the virtual environment specified by the `pyproject.toml`:

```bash
$ cd ~/itcoin-fbft/benchmark/
$ poetry shell
# NB: the next instruction may ask you to authenticate using the system keyring. This is an open poetry issue.
# See https://github.com/python-poetry/poetry/issues/1917
$ poetry install
```

## Run the benchmark experiments locally

Before running a local test, ensure that itcoin-fbft has been built. You can
refer to the [readme](../README.md). When running on AWS (see later) this will
not be necessary.

```bash
$ cd ~/itcoin-fbft/benchmark/
$ poetry shell
$ fab local <test-configuration-json-file> <output_folder>
```

where

- `<test-configuration-json-file>` is a json file containing the information about the test we want to run. You can find example configurations in `config-examples` folder.
- `<output_folder>` is the folder where we want to save the logs and the results

For example:

```bash
$ cd ~/itcoin-fbft/benchmark/
$ poetry shell
$ fab local config-examples/test-with-no-load.json results
```

## Running the Benchmark Framework on AWS

### Setup AWS

1. Install the AWS CLI by following the instructions in [Installing or updating the latest version of the AWS CLI](https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html)

2. Create a file `~/.aws/credentials` with the following content:

```c
[default]
aws_access_key_id = <YOUR_ACCESS_KEY_ID>
aws_secret_access_key = <YOUR_SECRET_ACCESS_KEY>
```

3. Add your SSH public key to your AWS account

You must now [add your SSH public key to your AWS account](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/ec2-key-pairs.html). This operation is manual and needs to be repeated for each AWS region that you plan to use. Upon importing your key, AWS requires you to choose a 'name' for your key; ensure you set the same name on all AWS regions. This SSH key will be used by the python scripts to execute commands and upload/download files to your AWS instances. If you don't have an SSH key, you can create one using [ssh-keygen](https://www.ssh.com/ssh/keygen/):

```bash
$ ssh-keygen -f ~/.ssh/aws
```

You can then import the new key in all the regions you plan to use by running:

```bash
AWS_REGIONS=("<region1>" "<region2>")
for each_region in ${AWS_REGIONS} ; do aws ec2 import-key-pair --key-name <keyPairName> --public-key-material fileb://$HOME/.ssh/aws --region $each_region ; done
```

### Configure the testbed

The file `itcoin-fbft/benchmark/settings.json` contains all the configuration parameters of the testbed to deploy. Its content looks as follow:

```json
{
    "testbed": "itcoin-testbed",
    "key": {
        "name": <keyPairName>,
        "path": "~/.ssh/aws"
    },
    "ports": {
        "consensus": 13000,
        "bitcoind": 38000
    },
    "repo": {
        "name": "itcoin-fbft",
        "url": "https://github.com/bancaditalia/itcoin-fbft.git",
        "branch": "main",
        "cmake_options": "-DITCOIN_CORE_GIT_REV=itcoin"
    },
    "instances": {
        "type": "t3.xlarge",
        "regions": [
            "eu-west-1", "eu-central-1"
        ]
    }
}
```

where:

- The testbed name (i.e., `itcoin-testbed`) is used to tag the name of the AWS instance for easy reference.

- The first block (i.e., `key`) contains information regarding your SSH key, and `<keyPairName>` must be the same one you used at step 3 of the "Setup AWS" section.

- The second block (`ports`) specifies the TCP ports to use. Itcoin-fbft needs 2 ports; the first is used to exchange consensus messages between the validators, the second one is used by the itcoin-core deamon. Note that these ports must be open to the WAN on all your AWS instances.

- The third block (`repo`) contains the information regarding the repository's name, the URL of the repo, and the branch containing the code to deploy. Modifying the branch name is particularly useful when testing new functionalities without having to checkout the code locally.

- The the last block (`instances`) specifies the [AWS instance type](https://aws.amazon.com/ec2/instance-types) and the [AWS regions](https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/using-regions-availability-zones.html#concepts-available-regions) to use. The instance type selects the hardware on which to deploy the testbed. For example, `t3.xlarge` instances come with 4 vCPUs, 16 GB of RAM, and guarantee 5 Gbps of bandwidth. The python scripts will configure each instance with 32 GB of SSD hard drive. The `regions` field specifies the data centers to use. If you require more nodes than data centers, the python scripts will distribute the nodes as equally as possible amongst the data centers. All machines run a fresh install of Ubuntu Server 22.04.

### Create a testbed

The AWS instances are orchestrated with [Fabric](http://www.fabfile.org/) from the file `itcoin-fbft/benchmark/fabfile.py`; you can list all possible commands as follows:

```bash
$ cd ~/itcoin-fbft/benchmark
$ poetry shell
$ fab --list
```

The command `fab create` creates new AWS instances; open [fabfile.py](http://fabfile.py) and locate the `create` task:

```python
@task
def create(ctx, nodes=1, clients=1):
    ''' Create a testbed'''
    ...
```

The parameters `nodes` determines how many VALIDATOR instances to create in each AWS region. That is, if you specified 5 AWS regions, setting `--nodes=2` will create a total of 10 VALIDATOR machines. The parameter `clients` determines how many CLIENT instances to create in each AWS region.

```bash
$ fab create --nodes=2 --clients=1
Creating 4 instances of type validator |██████████████████████████████| 100.0%
Waiting for all instances to boot...
Successfully created 4 new instances of type validator
Creating 2 instances of type client |██████████████████████████████| 100.0%
Waiting for all instances to boot...
Successfully created 2 new instances of type client
```

At any time, you can query the available machines, by running `fab info`:

```
$ fab info
----------------------------------------------------------------
    INFO:
----------------------------------------------------------------
    VALIDATOR Available machines: 4

    Region: EU-WEST-1
    0	ssh -i ~/.ssh/aws ubuntu@34.242.147.255
    1	ssh -i ~/.ssh/aws ubuntu@34.248.24.84

    Region: EU-CENTRAL-1
    0	ssh -i ~/.ssh/aws ubuntu@3.65.25.216
    1	ssh -i ~/.ssh/aws ubuntu@3.121.127.43
----------------------------------------------------------------

----------------------------------------------------------------
    INFO:
----------------------------------------------------------------
    CLIENT Available machines: 2

    Region: EU-WEST-1
    0	ssh -i ~/.ssh/aws ubuntu@54.75.6.173

    Region: EU-CENTRAL-1
    0	ssh -i ~/.ssh/aws ubuntu@3.70.199.156
----------------------------------------------------------------
```

You can then clone and install the repo on the remote instances with `fab install <settings.json>`. A default `<settings_file>` is available at `itcoin-fbft/benchamrk/settings.json`.

```bash
$ fab install settings.json
[...]
Initialized testbed of 6 nodes
```

### Start testbed and launch a remote test

If not already running, you can start the testbed by running:

```bash
$ fab start
```

Run a remote test using:

```bash
$ fab remote <test-configuration-json-file> <output_folder>
```

For example:

```bash
$ cd ~/itcoin-fbft/benchmark/
$ poetry shell
$ fab remote config-examples/test-with-no-load.json results
```

### Stop and terminate the testbed

You can stop and terminate the testbed by running:

```
$ fab stop
$ fab destroy
```

## Run a testbed in an entire folder

```bash
$ fab run-all <test-configuration-folder> <is_local> <output_folder>
```

where:

- `<test-configuration-folder>` is a folder containing one or more test configuration json files.
- `<is_local>` is a flag to indicate whether to run the simulations locally or remotely
- `<output_folder>` is the folder where we want to save the logs and the results.

## Debugging the Benchmark Framework with VSCode

Get the fab path and Python path of the virtual env, using the `which` command:

```bash
$ cd ~/itcoin-fbft/benchmark/
$ poetry shell
$ which fab
$ which python
```

In the `~/itcoin-fbft/.vscode/launch.json`  file add the following configuration:

```json
{
    "name": "Python: Benchmarking framework",
    "type": "python",
    "request": "launch",
    "python": <python_path>,
    "program": <fab_path>,
    "args": <fab_task>,
    "cwd": "${workspaceFolder}/benchmark",
    "console": "integratedTerminal"
}
```

In `args` specify the fabric task you want to run. The list of possible tasks can be seen running the command `fab --list`. For example:

`"args": ["local", "config-examples/experiment-0.json", "simulation_1"]`
