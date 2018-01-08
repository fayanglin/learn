# learn
echo "# learn" >> README.md
git init
git add README.md
git commit -m "first commit"
git remote add origin https://github.com/fayanglin/learn.git
git push -u origin master
如果你已经在本地创建了一个Git仓库，又想在GitHub创建一个Git仓库，并且让这两个仓库进行远程同步，那就需要用到SSH Key，github拿到了你的公钥就会知道内容是你推送的。

 

SSH Key的配置：

1.Windows下打开Git Bash，创建SSH Key，按提示输入密码，可以不填密码一路回车

$ ssh-keygen -t rsa -C "注册邮箱"
然后用户主目录/.ssh/下有两个文件，id_rsa是私钥，id_rsa.pub是公钥

 

2.获取key，打开.ssh下的id_rsa.pub文件，里面的内容就是key的内容

$ start ~/.ssh/id_rsa.pub
 

3.登录GitHub，打开"SSH Keys"页面，快捷地址：https://github.com/settings/ssh 



4.测试ssh key是否成功，使用命令“ssh -T git@github.com”，如果出现You’ve successfully authenticated, but GitHub does not provide shell access 。这就表示已成功连上github。

 

远程库与本地库之间的操作：

1.从远程克隆一份到本地可以通过git clone

Git支持HTTPS和SSH协议，SSH速度更快

$ git clone git@github.com:nanfei9330/xx.git
 

2.本地库关联远程库，在本地仓库目录运行命令：

$ git remote add origin git@github.com:nanfei9330/learngit.git
请替换为自己仓库的的SSH



 

3.推送master分支的所有内容

$ git push -u origin master
第一次使用加上了-u参数，是推送内容并关联分支。

推送成功后就可以看到远程和本地的内容一模一样，下次只要本地作了提交，就可以通过命令：

$ git push origin master
把最新内容推送到Github

 

=================实战一下吧======================

本地创建文本test.txt，运行:

$ git add text.txt
$ git commit -m"添加新文件"
$ git push origin master
然后就可以在github看到同步了

 

 

其他：

取回远程主机某个分支的更新，如

$ git pull origin master
