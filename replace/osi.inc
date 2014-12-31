#!/bin/sh
#set -x
###############################################################################################
#function:one step install
#time:2014-09-18
#author:zhouwei13
###############################################################################################
#规则：
#1. tar包需放在与业务相关目录同一层，解压后会在同一层创建一个以版本号命名的目录
#2. 目录中包括代码包/升级脚本（如：install.sh）/脚本配置文件（如：install.properties）/版本文件
#3. 代码包名称/版本文件等都需要写入配置文件，并且此脚本会做校验
#4. 版本文件中的版本信息需要和解压下来的目录名称一致，此脚本会做校验
#5. 详细的说明见下面各功能模块的描述
###############################################################################################

#installfile='install.properties'
configfile='config.properties'
#versionfile='.version'
#service_name='crm cs ucp web'

#proname=$1
#parameter1=$2
date1=`date +'%Y-%m-%d %H:%M:%S'`
#date2=`date +%Y%m%d%H%M%S`
#dir1=`pwd`
#dir2=`dirname $PWD`
#backuplist=`cat ${installfile} | awk -v pname=${proname}.backupdir -F\= '$0~"^"pname{print $2}'`
#vfile=`cat ${installfile} | awk -v pname=${proname}.versiondir -v hdir=${dir2} -F\= '$0~"^"pname{print(hdir"/"$2"/.version")}'`

#log
#记录日志，方便调用
log(){
echo ${date1} [$1] $2
#echo ${date1} [$1] $2 >> ${dir1}/update.log
}

#check files
#检查升级包文件，包括代码包/配置文件/版本文件
#其中：
#代码包检查支持多个代码包，配置文件中多个代码包用 空格 分开
#配置文件检查配置文件是否存在，配置文件的名称是否是此脚本中配置的名称
#版本文件检查包括检查版本文件是否存在，名称必须和此脚本中配置相同，版本文件中的版本信息必须和tar包名称相同（解压下来的目录名仍然需要时版本名称）
checkfiles(){
if [ -f ${installfile} ];then
	log "INFO" "installfile ${installfile} check complete."
else
	log "ERROR" "installfile ${installfile} is not exist or filename is wrong,please check."
	exit 1
fi

if [ -f ${configfile} ];then
	log "INFO" "configfile ${configfile} check complete."
else
	log "ERROR" "configfile ${configfile} is not exist or filename is wrong,please check."
	exit 1
fi

packagename=`cat ${installfile} | awk -v pname=${proname}.package -F\= '$0~"^"pname{print $2}'`
for i in ${packagename}
do
	if [ -f ${i} ];then
		log "INFO" "packagefile ${i} check complete."
	else
		log "ERROR" "packagefile ${i} is not exist,please check."
		exit 1
	fi
done

if [ -f ${versionfile} ];then
	newversion=`cat .version`
	packageversion=`basename ${dir1}`
	if [ "${newversion}" == "${packageversion}" ];then
		log "INFO" "versionfile ${versionfile} check complete."
	else
		log "ERROR" "versionfile information and packagename are not equal,please check."
		exit 1
	fi
else
	log "ERROR" "versionfile ${versionfile} is not exist or filename is wrong,please check."
	exit 1
fi
}

#check module
#检查输入的模块参数是否是crm/cs/ucp/web
checkmodule(){
if [[ ${service_name} =~ ${proname} ]];then
	log "INFO" "input module is correct."
else
	log "ERROR" "input module is wrong,please input:crm/cs/ucp/web."
	exit 1
fi
}

#check disk space
#检查磁盘空间，其中：
#剩余空间为当前目录所在磁盘分区的剩余空间
#预计需要使用的空间包括：当前目录所占空间大小/需要备份的目录大小除以2（此处除以2是预估压缩打包后的文件大小占原目录大小的一半，即 压缩效率）
checkdf(){
a=`df . | tail -1 | awk '{print $(NF-2)}'`
b=`du -s . | awk '{print $1}'`
c=`cd ${dir2};du -s ${backuplist} | awk '{sum+=$1}END {print sum}';cd ${dir1}`
d=$((a-b-c/2))

if [ $d -gt 0 ];then
	log "INFO" "disk space is enough."
else
	log "ERROR" "disk space is not enough,please check."
	exit 1
fi
}

#backup
#备份
backup(){
for i in ${backuplist}
do
	cd ${dir2}/${i}
	backupfilename=`pwd | sed 's/\//_/g'`
	tar -zcf ${dir1}/backup${backupfilename}_${date2}.tar.gz ./*
	cd ${dir1}
	log "INFO" "directory $i backup complete."
done
}

#update
#1. 检查本地是否有版本信息文件，如果有则追加写入本次版本号，如果没有则默认将版本信息文件放在配置文件中配置的目录
#2. 版本检查目前暂定"version"命令，此为快捷命令，会读取版本信息文件最后一行
#3. 实际的升级过程依赖于配置文件中的command，此脚本会读取各command值，并以此执行（没有写死升级命令，是因为各应用的升级方法不一样，通过配置文件执行较为灵活）
update(){
if [ -f ${vfile} ];then
	cat ${versionfile} >> ${vfile}
else
	cp ${versionfile} ${vfile}
fi

valias=`cat ${dir2}/.cshrc | awk '$1~/^alias/ && $2~/version/{print $2}'`

if [ "${valias}"x != "version"x ];then
	echo "alias version 'cat ${vfile} | tail -1'" >> ${dir2}/.cshrc
	log "INFO" "alias version add complete,become effective after relogin."
else
	log "INFO" "alias version already exists."
fi

cat ${installfile} | awk -v cname=${proname}.command -F\= '$0~"^"cname{print $2}' | while read i
do
	echo `${i} &` > /dev/null 2>&1
	log "INFO" "command ${i} executed successfully."
done
}
#modifyconfig(对非jar文件预处理)
modifyconfig(){
#$1=crm_21_0.ear
#$2=crmappc0
type=`echo $1|awk -F '[.]' '{print $2}'`
rm -rf tmp/*
case $type in
"jar")
	cp packages/$1 tmp
	ls tmp
	modifyconfig_jar $2
	if [ "$?" == "0" ];then
		cp -rf tmp/$1 dest
		echo "替换成功"
		exit 0
	else 
		echo "替换失败"
		exit 0
	fi
	;;
"ear")
	cd tmp;jar -xf ../packages/$1;cd ..
	modifyconfig_jar $2
	if [ "$?" == "0" ];then
		cd tmp;jar -cf $1 *;cp -rf $1 ../dest
		echo "替换成功" 
		exit 0
	else 
		echo "替换失败"
		exit 0
	fi	
	;;
"war")
	cd tmp;jar -xf ../packages/$1;cd ..
	modifyconfig_jar $2 && modifyconfig_unjar $2
	if [ "$?" == "0" ];then
		cd tmp;jar -cf $1 *;cp -rf $1 ../dest
		echo "替换成功" 
		exit 0
	else 
		echo "替换失败"
		exit 0
	fi
	;;
*)
	echo "文件名不正确"
	;;
esac
	
}
#modifyconfig_jar(包含解压jar包&modifyconfig模块中unjar部分)
#每次只能在参数中指定一个proname
modifyconfig_jar(){

#set -x
#JAVA_HOME=./java
#export JAVA_HOME
#PATH=$PATH:$JAVA_HOME/bin
#export PATH

proname=$1
echo "#####$proname"
jarpath=`cat ${configfile} | grep ^${proname}.cfgfile.jar.jarname | awk '{print(substr($0,index($0,"=")+1))}'`
jarname=`basename ${jarpath}`
mkdir ${proname}_temp;cd ${proname}_temp
JAVA_HOME=./java
export JAVA_HOME
PATH=$PATH:$JAVA_HOME/bin
export PATH
jar -xf ../tmp/${jarpath}
	cnt1=`cat ../${configfile} | grep ^${proname}.cfgfile.jar.filename | wc -l`
	if [ ${cnt1} -gt 0 ];then
		for((b=1;b<=${cnt1};b++))
		do
			x1=`cat ../${configfile} | grep ^${proname}.cfgfile.jar.filename${b}= | awk '{print(substr($0,index($0,"=")+1))}'`
			y1=`cat ../${configfile} | grep ^${proname}.cfgfile.jar.value${b}.from | awk '{print(substr($0,index($0,"=")+1))}'`
			z1=`cat ../${configfile} | grep ^${proname}.cfgfile.jar.value${b}.to | awk '{print(substr($0,index($0,"=")+1))}'`
			[ 0 = `grep -c $y1 $x1` ] && log "ERROR" "cfgfile NO.${b} ${x1} no such from.value."
			sed -i s#${y1}#${z1}#g ${x1}
			[ 0 = `grep -c $z1 $x1` ] && log "ERROR" "cfgfile NO.${b} ${x1} replace the to.value failed."
			log "INFO" "cfgfile NO.${b} ${x1} modified successfully."
		done
	else
		log "INFO" "no unjar_cfgfile need to be modified."
	fi
jar -cf ${jarname} *
mv ${jarname} ../tmp/${jarpath}
cd ..;rm -rf ${proname}_temp
echo "modifyconfig sucessful"
return "0"
}
modifyconfig_unjar(){
proname=$1
cd tmp
	cnt1=`cat ../${configfile} | grep ^${proname}.cfgfile.unjar.filename | wc -l`
	if [ ${cnt1} -gt 0 ];then
		for((b=1;b<=${cnt1};b++))
		do
			x1=`cat ../${configfile} | grep ^${proname}.cfgfile.unjar.filename${b}= | awk '{print(substr($0,index($0,"=")+1))}'`
			y1=`cat ../${configfile} | grep ^${proname}.cfgfile.unjar.value${b}.from | awk '{print(substr($0,index($0,"=")+1))}'`
			z1=`cat ../${configfile} | grep ^${proname}.cfgfile.unjar.value${b}.to | awk '{print(substr($0,index($0,"=")+1))}'`
			[ 0 = `grep -c $y1 $x1` ] && log "ERROR" "cfgfile NO.${b} ${x1} no such from.value."
			sed -i s#${y1}#${z1}#g ${x1}
			[ 0 = `grep -c $z1 $x1` ] && log "ERROR" "cfgfile NO.${b} ${x1} replace the to.value failed."
			log "INFO" "cfgfile NO.${b} ${x1} modified successfully."
		done
	else
		log "INFO" "no unjar_cfgfile need to be modified."
	fi
cd ..
echo "modifyconfig sucessful"
return "0"
}
#modifyconfig(暂时不用)
#1. 修改配置文件放在发布完成之后
#2. unjar类型filepath只需要配置自config目下的相对路径
modifyconfig1(){
for a in ${proname}
do
	cnt1=`cat ../${configfile} | grep ^${proname}.cfgfile.unjar.filename | wc -l`
	if [ ${cnt1} -gt 0 ];then
		for((b=1;b<=${cnt1};b++))
		do
			x1=`cat ../${configfile} | grep ${proname}.cfgfile.unjar.filename${b} | awk '{print(substr($0,index($0,"=")+1))}'`
			y1=`cat ../${configfile} | grep ${proname}.cfgfile.unjar.value${b}.from | awk '{print(substr($0,index($0,"=")+1))}'`
			z1=`cat ../${configfile} | grep ${proname}.cfgfile.unjar.value${b}.to | awk '{print(substr($0,index($0,"=")+1))}'`
			sed -i s#${y1}#${z1}#g ${x1}
			log "INFO" "cfgfile ${x1} modified successfully."
		done
	else
		log "INFO" "no unjar_cfgfile need to be modified."
	fi
	
	cnt2=`cat ${configfile} | grep ^${a}.cfgfile.jar.filename | wc -l`
	if [ ${cnt2} -gt 0 ];then
		for((c=1;c<=${cnt2};c++))
		do
			x2=`cat ${configfile} | grep ${a}.cfgfile.jar.filename${c} | awk '{print(substr($0,index($0,"=")+1))}'`
			y2=`cat ${configfile} | grep ${a}.cfgfile.jar.value${c}.from | awk '{print(substr($0,index($0,"=")+1))}'`
			z2=`cat ${configfile} | grep ${a}.cfgfile.jar.value${c}.to | awk '{print(substr($0,index($0,"=")+1))}'`
			w2=`cat ${configfile} | grep ${a}.cfgfile.jar.path${c} | awk '{print(substr($0,index($0,"=")+1))}'`
			s2=`echo ${x2} | awk -F\/ '{print($NF)}'`
			f2=`echo ${a}jarfilename${c}`
			mkdir ${f2};cd ${f2}
			jar -xf ../${x2}
			sed -i s#${y2}#${z2}#g ${w2}
			jar -cf ${s2} *
			mv ${s2} ../${x2}
			cd ..;rm -rf ${f2}
			log "INFO" "cfgfile ${x2} modified successfully."
		done
	else
		log "INFO" "no jar_cfgfile need to be modified."
		exit 1
	fi
done
}

#rollback
#1. 回滚依赖于之前已对相应目录进行备份（升级的时候会根据配置文件中的配置目录进行备份）
#2. 回滚仅将备份的目录删除，并将之前的备份包解压到对应目录
#3. rollback模块的执行依赖于执行脚本的时候是否输入"rollback"参数，在下面会有校验
rollback(){
for i in ${backuplist}
do
	cd ${dir2}/${i}
	backupfilename=`pwd | sed 's/\//_/g'`
	backupfilecnt=`ls ${dir1}/backup${backupfilename}_*.tar.gz | wc -l`
	if [ ${backupfilecnt} != 0 ];then
		if [ ${backupfilecnt} = 1 ];then
			rm -rf *
			tar -zxf ${dir1}/backup${backupfilename}_*.tar.gz -C ./
			cd ${dir1}
			log "INFO" "directory $i rollback complete."
		else
			log "ERROR" "more than one backup files exist,please remove excess files."
			exit 1
		fi
	else
		log "ERROR" "${dir1}/backup${backupfilename}_*.tar.gz:no such file,please check."
		exit 1
	fi
done
}

