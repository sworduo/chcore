## ChCore实验代码使用教程

### 1.获取代码、虚拟机

实验源代码获取：好大学在线-操作系统

下载好ChCore实验的源代码压缩包后，请在本地解压缩，并进入实验根目录：

```shell
~$ tar -xvf chcore-lab.tar.gz
~$ cd chcore-lab
```

同时，我们也提供了配置好环境的虚拟机：
[VirtualBox](https://jbox.sjtu.edu.cn/l/xncuyo)和[VMware](https://jbox.sjtu.edu.cn/l/UHkDo1)，
下载密码都是os2020。

虚拟机中用户名都是os，密码是123。

### 2.使用Git管理代码

Git是一个非常便捷的代码版本管理工具，实验源代码使用进行管理，对于Git操作不熟悉的同学可以通过[gittutorial](https://git-scm.com/docs/gittutorial)文档对Git的命令有初步了解。同时，本文档也会给出完成实验所必需的Git命令。

#### 2.1.连接远端仓库

实验源代码是以本地Git仓库（repository）的形式呈现的，我们创建在远端创建仓库，并将本地仓库推送（push）到远端。使用远端仓库的好处是，避免对于实验文件的误删除、电脑损坏等不可抗力导致同学完成的实验代码丢失。我们假设使用的远端Git服务提供者是[github](https://www.github.com)，用户名为"username"，仓库名为"chcore-lab"，具体流程如下：

1. 注册github账户"username"，并创建一个新的仓库"chcore-lab"，获取仓库链接“https://github.com/username/chcore-lab.git
2. 在本地命令行运行：

```shell
~$ git branch -M lab1
~$ git remote add origin https://github.com/username/chcore-lab.git
~$ git push --all --set-upstream origin
```

至此，一个远端仓库就已经创建完成了。

##### 2.1.1从远端仓库下载代码

如果希望从远端仓库下载自己的实验代码，可以在本地命令行运行：

```shell
~$ git clone https://github.com/username/chcore-lab.git
```

该命令执行完后，会自动在当前目录的“chcore-lab”下创建。

#### 2.2. 创建新的实验分支

在本地仓库中，实验代码共有若干个分支（branch）。每个分支对应了一个实验，例如分支"lab1"对应了实验一的代码。为了查看当前仓库中存在的所有分支，可以输入`git branch`。

为了避免覆盖初始的实验框架，我们推荐在开始完成每个实验时，创建新的分支用于作答。例如在最开始，为了完成实验一，需要创建分支"lab1"的新分支"lab1-sol"，并将其推送到远端仓库。

```shell
~$ git checkout -b lab1-sol
~$ git push --set-upstream origin lab1-sol
```

#### 2.3. 提交实验的修改

在每完成实验的一个练习时，我们鼓励将当前的修改提交（commit），其目的与连接远端仓库相同。

每当完成一个练习、一个重要功能点、或解决了一个bug时，请运行以下命令：

```shell
~$ git add --all
~$ git commit -m "commit message"
```

`git add`会记录从上一个提交开始后所有的代码修改。有时候，可能部分修改是临时的，因而并不应该被记录。同学也可以使用`git add xxx`，表示将指定的"xxx"目录或文件记录。

`git commit`命令会将被记录的代码作为一个版本提交，其中`-m`后的字符串是提交信息，可以替换为"finish lab 1 exercise 1"、“fix xxx bug“等，用于辅助理解这个版本的代码进度。为了浏览提交历史，可以在本地命令行输入`git log`查看。

然而，此时这个代码版本仍然在本地，所以需要运行

```shell
~$ git push
```

将这个代码版本推送到远端仓库持久化。

#### 2.4 完成实验，切换代码分支

由于实验之间有依赖关系，所以需要按顺序完成每个实验的代码，并将当前分支上的修改合并到后续实验的分支上，才能开始后续实验的练习。假设现在同学已经完成了实验`{x}`（即第x个实验，在后续的代码中，请相应地替换`{x}`与`{x+1}`），那么应该执行以下步骤进而开始下一个实验的练习。

##### 2.4.1 提交修改

我们首先提交修改，操作与第2.3节所介绍的一样。

```shell
~$ git add --all
~$ git commit -m "finish lab{x}"
~$ git push
```

##### 2.4.2 创建新实验的分支

接着，我们创建新实验`{x+1}`用于作答的分支。我们需要首先切换到实验`{x+1}`的原分支：

```shell
~$ git checkout lab{x+1}
```

然后创建一个新分支"lab{x+1}-sol"用于作答：

```shell
~$ git checkout -b lab{x+1}-sol
```

##### 2.4.3 合并之前的实验代码

由于分支"lab{x+1}"提供的代码里是不包含任何答案的，我们需要将我们到上一个实验为止的所有代码修改（即分支"lab{x}-sol"）都合并（merge）到当前分支个"lab{x+1}-sol"上。

```shell
~$ git merge lab{x}-sol
```

如果命令行提示合并产生了冲突，则需要手动解决冲突，然后将冲突提交。

在冲突被解决后，我们将修改再次提交修改：

```shell
~$ git add --all
~$ git commit -m "finish merge lab{x}"
```

然后，将新创建的分支推送到远端仓库

```shell
~$ git push --set-upstream origin lab{x+1}-sol
```

上一步完成后，我们就可以在新的实验代码分支上开始作答了。

### 3. 开始完成实验

至此，我们已经了解在完成ChCore实验时基本所需的Git命令，预祝各位完成实验顺利。

