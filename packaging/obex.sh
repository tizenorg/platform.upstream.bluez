#!/bin/sh

if [ ! -z `ps ax | grep -v grep | grep obexd` ];
then return
fi

eval $(tzplatform-get TZ_USER_CONTENT);

if [ ! -d $TZ_USER_CONTENT ];
then mkdir -p $TZ_USER_CONTENT;
fi

exec /lib/bluetooth/obexd -d --noplugin=syncevolution,pcsuite,irmc --symlinks -r $TZ_USER_CONTENT;

