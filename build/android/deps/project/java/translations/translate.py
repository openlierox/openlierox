#!/usr/bin/env python
from urllib2 import urlopen
from urllib import urlencode
import sys

# The google translate API can be found here:
# http://code.google.com/apis/ajaxlanguage/documentation/#Examples

lang1=sys.argv[1]
lang2=sys.argv[2]
langpair='%s|%s'%(lang1,lang2)
text=' '.join(sys.argv[3:])
base_url='http://ajax.googleapis.com/ajax/services/language/translate?'
params=urlencode( (('v',1.0),
('q',text),
('langpair',langpair),) )
url=base_url+params
content=urlopen(url).read()
start_idx=content.find('"translatedText":"')+18
translation=content[start_idx:]
end_idx=translation.find('"}, "')
translation=translation[:end_idx]
print translation

