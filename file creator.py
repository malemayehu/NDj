from os import listdir, getcwd
from os.path import isfile, join
import eyed3



cwd_string = getcwd()
print(cwd_string)

onlyfiles = listdir('C:\\Users\\hetaa\\Documents\\Programs\\NDj_music')
song_list = open(cwd_string + "\SDINIT.txt",'w+')
songarr = []
for i in onlyfiles:
        try:
            temp  = eyed3.load('C:\\Users\\hetaa\\Documents\\Programs\\NDj_music\\' + i)
            string = (temp.tag.title)
            songarr.append(str(string))
        except:
            print(i)
for i in range(len(songarr)):
        string = songarr[i] 
        print(string)
        for j in range(len(string)):
                  try:
                      temp = ord(string[j])
                      if not ((temp >= 65 and temp < 91) or (temp > 96 and temp < 123) or temp == 32 or temp == 33):
                              if( ord(string[j-1]) == 32):
                                      songarr[i] = string[:j-1]# remove space
                              else:
                                      songarr[i] = string[:j]
                              continue
                  except:
                       pass

songarr.sort()
        

                  

for i in songarr:
        song_list.write(i)
        song_list.write('\n')
song_list.close()
##test
