# HyperParaRC

HyperParaRC can run in `standalone` mode, which we can test the parallel repair without the integration to a distributed storage system, and a `HDFS3 integration` mode, which we can test parallel repair with Hadoop-3.3.4.



To run HyperParaRC in the standalone mode, we need to finish the following steps:

- Prepare a cluster in Alibaba Cloud
- Compile HyperParaRC 
- Prepare configuration files for each machine
- Generate a stripe of blocks
- Start HyperParaRC 
- Test parallel repair

To run HyperParaRC in the HDFS3 integration mode, we need to finish the following steps:

- Prepare a cluster in Alibaba Cloud *
- Compile HyperParaRC 
- Deploy Hadoop-3.3.4 with OpenEC *
- Prepare configuration files for each machine *
- Generate a stripe of blocks in Hadoop-3.3.4 *
- Start HyperParaRC 
- Test parallel repair



## 1. Install

We have implemented `HyperParaRC` on Ubuntu18.04 LTS.
 ### 1.1 Common

  - g++ & make & libtool & autoconf & git

  ```
  $ sudo apt update
  $ sudo apt-get install cmake g++ libtool autoconf git
  ```

  - gf-complete

  ```
  $ git clone https://github.com/ceph/gf-complete.git
  $ cd gf-complete
  $ ./autogen.sh && ./configure && make && sudo make install
  ```



`HyperParaRC` uses redis to implement transmission.

  - redis-3.2.8

  ```
  $ sudo wget http://download.redis.io/releases/redis-3.2.8.tar.gz
  $ tar -zxvf redis-3.2.8.tar.gz
  $ cd redis-3.2.8
  $ make && sudo make install
  ```

  Configure redis to be remotely accessible.

  ```
  $ sudo /etc/init.d/redis_6379 stop
  ```

  Edit /etc/redis/6379.conf. Find the line with bind 127.0.0.0 and modify it to bind 0.0.0.0, then start redis.

  ```
  $ sudo /etc/init.d/redis_6379 start
  ```



### Wondershaper

`HyperParaRC` needs [Wondershaper](https://github.com/magnific0/wondershaper) to limit the speed of network and complete the experiments.

```
$  git clone git://github.com/magnific0/wondershaper.git
$  cd wondershaper
$  sudo make install
```



### Intel®-storage-acceleration-library (ISA-L)

`HypreParaRC` uses [ISA-L](https://github.com/intel/isa-l)(Intel(R) Intelligent Storage Acceleration Library) to perform encoding operations.

```
$  git clone https://github.com/intel/isa-l.git
$  cd isa-l
$  ./autogen.sh
$  ./configure; make; sudo make install
```



### 1.2 Compile HyperParaRC

  After finishing the preparations above, download and compile the source code.

  ```
  $ cd HyperParaRC
  $ ./compile.sh
  ```





 ## 2. Standalone Mode

### 2.1 Prepare a cluster in Alibaba Cloud

###### Cluster Configuration

| Machine    | Number | Alibaba Machine Type | IP                                                           |
| ---------- | :----- | -------------------- | ------------------------------------------------------------ |
| Controller | 1      | ecs.r7.2xlarge       | 192.168.0.1                                                  |
| Agent      | 15     | ecs.r7.xlarge        | 192.168.0.2;192.168.0.3; 192.168.0.4; 192.168.0.5; 192.168.0.6; 192.168.0.7; 192.168.0.8; 192.1;68.0.9; 192.168.0.10; 192.168.0.11; 192.168.0.12; 192.168.0.13; 192.168.0.14; 192.168.0.15; 192.168.0.16; |

In each machine, we create a default username called `usr`.



 ### 2.2 Prerequisites

 #### 2.1.1 Configuration File

Each machine requires a configuration file `/home/usr/HyperParaRC/conf/sysSettings.xml`. We show the configruation parameters in the following table:

| Parameters              | Description                                                  | Example                                                      |
| ----------------------- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| controller.addr         | The IP address of the controller.                            | `192.168.0.1;`                                               |
| agents.addr             | The IP address of all agents.                                | `192.168.0.2; 192.168.0.3; 192.168.0.4; 192.168.0.5; 192.168.0.6; 192.168.0.7; 192.168.0.8; 192.1;68.0.9; 192.168.0.10; 192.168.0.11; 192.168.0.12; 192.168.0.13; 192.168.0.14; 192.168.0.15; 192.168.0.16` |
| local.addr              | The local IP address of a machine.                           | `192.168.0.2 ` for the controller; `192.168.0.3` for the first agent. |
| block.bytes             | The size of a block in bytes.                                | `268435456` for 256 MIB.                                     |
| packet.bytes            | The size of a packet in bytes.                               | `65536` for 64 KiB sub-packet.                               |
| code.name               | Type of an erasure code.                                     | `Clay`                                                       |
| code.ecn                | Erasure coding parameter n.                                  | `14` for (14,10) Clay code.                                  |
| code.eck                | Erasure coding parameter k.                                  | `10` for (14,10) Clay code.                                  |
| code.ecw                | Sub-packetization level.                                     | `256` for (14,10) Clay code.                                 |
| batch.size              | The number of stripes repaired in parallel.                  | `29` Defaulting to the number of available nodes in the cluster. |
| recvgroup.size          | The number of threads executing receive tasks in parallel on each agent. | `10`                                                         |
| sendgroup.size          | The number of threads executing send tasks in parallel on each agent. | `10`                                                         |
| computegroup.size       | The number of threads executing compute tasks in parallel on each agent. | `10`                                                         |
| block.directory         | The directory to store blocks.                               | `/home/usr/ParaRC/blkDir` for standalone mode; `/home/usr/hadoop-3.3.4-src/hadoop-dist/target/hadoop-3.3.4/dfs/data/current` for hdfs-3 integration mode. |
| stripestore.directory   | The directory to store stripe metadata.                      | `/home/usr/HyperParaRC/stripeStore`                          |
| tradeoffpoint.directory | The directory to store the MLP generated by PRS Generator offline. | `/home/usr/HyperParaRC/offline`                              |



Here is a sample of the configuration file in the controller in `/home/usr/HyperParaRC/conf/sysSettings.xml`

```xml
<setting>
<attribute><name>controller.addr</name><value>192.168.0.1</value></attribute>
<attribute><name>agents.addr</name>
<value>192.168.0.2</value>
<value>192.168.0.3</value>
<value>192.168.0.4</value>
<value>192.168.0.5</value>
<value>192.168.0.6</value>
<value>192.168.0.7</value>
<value>192.168.0.8</value>
<value>192.168.0.9</value>
<value>192.168.0.10</value>
<value>192.168.0.11</value>
<value>192.168.0.12</value>
<value>192.168.0.13</value>
<value>192.168.0.14</value>
<value>192.168.0.15</value>
<value>192.168.0.16</value>
</attribute>
<attribute><name>block.bytes</name><value>268435456</value></attribute>
<attribute><name>packet.bytes</name><value>65536</value></attribute>
<attribute><name>code.name</name><value>Clay</value></attribute>
<attribute><name>code.ecn</name><value>14</value></attribute>
<attribute><name>code.eck</name><value>10</value></attribute>
<attribute><name>code.ecw</name><value>256</value></attribute>
<attribute><name>batch.size</name><value>10</value></attribute>
<attribute><name>recvgroup.size</name><value>10</value></attribute>
<attribute><name>sendgroup.size</name><value>10</value></attribute>
<attribute><name>computegroup.size</name><value>10</value></attribute>
<attribute><name>local.addr</name><value>192.168.0.2</value></attribute>
<attribute><name>block.directory</name><value>/home/usr/HyperParaRC/blkDir</value></attribute>
<attribute><name>stripestore.directory</name><value>/home/usr/HyperParaRC/stripeStore</value></attribute>
<attribute><name>tradeoffpoint.directory</name><value>/home/usr/HyperParaRC/offline</value></attribute>
</setting>
```





### 2.3 Generate blocks

we need to configure the cluster information in `parafullnode/scripts/cluster`, and we assume a default cluster name called `default`.

- scripts/cluster/default
  - agents: The IP address of all agents.
  - controller: The IP address of all agents.

Here is a sample of the configuration file in the controller in `HyperParaRC/scripts/cluster/default/agents`

```
192.168.0.2
192.168.0.3
192.168.0.4
192.168.0.5
192.168.0.6
192.168.0.7
192.168.0.8
192.168.0.9
192.168.0.10
192.168.0.11
192.168.0.12
192.168.0.13
192.168.0.14
192.168.0.15
192.168.0.16
```

Here is a sample of the configuration file in the controller in `HyperParaRC/scripts/cluster/default/controller`

```
192.168.0.1
```



We invoke this script to generate blocks in block directory on each node, and store the placement in the `HyperParaRC/stripeStore/placement`.

```
$> python ./scripts/data/gen_standalone_encode_data.py  [cluster] [stripes num] [code] [n] [k] [w]  [blkMB] [failed node]
```

- cluster
  - The name of cluster
- code
  - The name of an erasure code.
  - For example, `Clay` for Clay codes.
- n
  - The erasure coding parameter n.
  - For example, `n=14` for (14,10) Clay code.
- k
  - The erasure coding parameter k.
  - For example, `k=10` for (14,10) Clay code.
- w
  - The sub-packetization level
  - For example, `w=256` for (14,10) Clay code.
- blkMB
  - The size of a block in MB.
  - For example, `blkMB=256` for the block size of 256 MiB.
- failed node
  - The indexor of the failed node in the cluster. The script ensures that there is a block stored on the failed node in each stripe, determining a fixed number of stripes involved in this full-node repair process.
  - For example, `failed node=0` for the first agent node which ip is `192.168.0.2`. 

For example, we generate 10 stripes of (14,10) Clay-coded with block size = 256MB stripe as follows:

```
$> python ./scripts/data/gen_standalone_encode_data.py demo 10 Clay 14 10 256 256 0
```



### 2.4 Run

#### Start HyperParaRC

The start script is in `HyperParaRC/scripts/.`

```
$ python scripts/start.py
```



#### Full-node Repair

We trigger the reconstruction of the failed node by invoking this command.

```
$ ./build/ParaCoordinator parallel scatter [fail node idx]
```

For example, we perform the full-node repair on the first agent node .

```
$ ./build/ParaCoordinator parallel scatter 0
```



#### Stop HyperParaRC

The stop script is in `HyperParaRC/scripts/.`

```
$ python scripts/stop.py
```



 ## 3. Hadoop3 integration mode 

The HDFS3 integration of HyperParaRC is built with OpenEC atop Hadoop-3.3.4. Note that only the steps marked with * are different with those in standalone mode. We focus on the steps marked with * in this part.

- Prepare a cluster in Alibaba Cloud *
- Compile HyperParaRC 
- Deploy Hadoop-3.3.4 with OpenEC *
- Prepare configuration files for each machine *
- Generate a stripe of blocks in Hadoop-3.3.4 *
- Start HyperParaRC
- Test parallel repair

### 3.1 Prepare a cluster in Alibaba Cloud

We can use the same cluster we applied in the standalone mode. The following tables shows how we deploy run `HyperParaRC` with Hadoop-3.3.4, OpenEC.

| HyperParaRC | OpenEC     | Hadoop    |
| ----------- | ---------- | --------- |
| Controller  | Controller | NameNode  |
| Agents      | Agents     | DataNodes |



### 3.2 Deploy Hadoop-3.3.4 with OpenEC

In the source code of HyperParaRC, we have a patch for OpenEC:

```
$> ls openec-pararc-patch
hdfs3-integration  openec-patch
```

- hdfs3-integration
  - Source code patch and installation scripts for Hadoop-3.3.4
- openec-patch
  - Source code patch for OpenEC

We first deploy Hadoop-3.3.4 in the cluster, then follows OpenEC.



###### 3.2.1 Build Hadoop-3.3.4 with HyperParaRC patch

We first download the source code of Hadoop-3.3.4 in `/home/usr/hadoop-3.3.4-src` in the `NameNode`. Then we install Hadoop-3.3.4:

```
$> cd /home/usr/HyperParaRC/openec-pararc-patch/hdfs3-integration
$> ./install.sh
```

After running the `install.sh`, we copy the patch to the source code of Hadoop-3.3.4 and compile the source code of Hadoop-3.3.4.



###### 3.2.2 Configure Hadoop-3.3.4

We follow the document of the configuration for Hadoop-3.0.0 in OpenEC to configure Hadoop-3.3.4. We show the difference here:

- As the default username is `usr`, please change accordingly in configuration files.

- Please use the IPs generated in Alibaba Cloud in your account when configuring Hadoop-3.3.4

- In `hdfs-site.xml`

  | Parameter     | Description                                                  | Example                                                      |
  | ------------- | ------------------------------------------------------------ | ------------------------------------------------------------ |
  | dfs.blocksize | The size of a block in bytes.                                | 268435456 for block size with 256 MiB.                       |
  | oec.pktsize   | The size of a packet in bytes. Note that for a MSR code with sub-packetization level w, the size of a packet is w times the size of a sub-packet. | 16777216 for (14,10) Clay code, where w=256, sub-packet size is 64 KiB. |

After we generate all the configuration files, please follow the document for Hadoop-3.0.0 provided in OpenEC to deploy Hadoop-3.3.4 in the cluster.



###### Build OpenEC with HyperParaRC patch

We first download the source code of OpenEC in `/home/usr/openec` in the `Controller`. Then we install the HyperParaRC patch with OpenEC.

```
$> cd /home/usr/HyperParaRC/openec-pararc-patch/openec-patch
$> cp -r ./* /home/usr/openec/src
$> cd /home/usr/openec/
$> cmake . -DFS TYPE:STRING=HDFS3
$> make
```

Now we have built the source code of OpenEC with HyperParaRC patch.



###### Configure OpenEC

We follow the document of the configuration in OpenEC with the following differences:

- ec.policy

  | Parameter | Description                                                  | Example    |
  | --------- | ------------------------------------------------------------ | ---------- |
  | ecid      | Unique id for an erasure code.                               | clay_14_10 |
  | class     | Class name of erasure code implementation.                   | Clay       |
  | n         | Parameter n for the erasure code.                            | 14         |
  | k         | Parameter k for the erasure code.                            | 10         |
  | w         | Sub-packetization level.                                     | 256        |
  | opt       | Optimization level for OpenEC . We do not use optimization in OpenEC. | -1         |

- As the default username is `usr`, please change accordingly in configuration files.

- Please use the IPs generated in Alibaba Cloud in your account when configuring OpenEC.

After we generate the configuration files, please follow the document in OpenEC to deploy OpenEC in the cluster.



### Prepare configuration files for each machine

In the configuration files of `HyperParaRC`, we have the following differences compared with the configuration in the standalone mode.

| Parameters      | Description                    | Example                                                      |
| --------------- | ------------------------------ | ------------------------------------------------------------ |
| block.directory | The directory to store blocks. | `/home/usr/hadoop-3.3.4-src/hadoop-dist/target/hadoop-3.3.4/dfs/data/current` for hdfs-3 integration mode. |



#### Generate a stripe of blocks in Hadoop-3.3.4

We prepare a script `scripts/hdfs/parafullnodetest.py` to execute block generation in HDFS-3.3.4, generate metadata files, and trigger the repair request.

The usage of this script is:

```
$> python scripts/parafullnodetest.py [cluster] [code] [n] [k] [w] [blkMB] [pktKB] [nStripes] [groupsize] [gen_files] [gen_blocks] [gen_meta]
```

- cluster

  - The name of cluster

- code

  - The name of the erasure code.

- n

  - The erasure coding parameter n.

- k

  - The erasure coding parameter k.

- w

  - The sub-packetization level.

- blkMB

  - The size of a block in MiB.
  - For example, `blkMB=256` for the block size of 256 MiB.

- pktKB

  - The size of a packet in KiB.
  - For example, `pktKB=64` for the packet size of 64 KiB.

- nStripes

  - The number of stripes to generate.

- groupsize

  - The number of stripes to repair in parallel in a group.

- Example

  ```
  $> python scripts/hdfs/parafullnodetest.py default Clay 14 10 256 256 64 10 5
  ```

# Multiple
