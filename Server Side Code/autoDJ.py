import requests 
import operator
# first get ID for each track 
def get_track_id(track_name):
	""" gets track id given a track name 
		input: track name as a string 
		output: track id (some number)
	"""
	r = requests.get('https://api.spotify.com/v1/search?q={}&type=track'.format(track_name))
	track_id = r.json()['tracks']['items'][0]['id']
	return track_id

#use track IDs to obtain Audio Features for each track 
def get_track_features(track_id):
	"""get track features json 
		input: track id
		output: track-specific json 
	"""
	#token = 'BQBiKCmGN8ADoZ9zMUP3nfrsCUiYcSiqUUk8KdZZiWlf5O-gaqIReMEaUXiTkDI3tcC7uVeMyVSLK0WEWPUbdnAogaJ7vlZEHEeAGjsSiApAEt84P5nrYqoB_3SFeEMTiWmp5ar4T9c'
	token = ''
	headers = {'Authorization': 'Bearer '+token}
	url = "https://api.spotify.com/v1/audio-features/{}".format(track_id)
	r = requests.get( url,headers = headers)
	return r.json()

def get_danceability(track_id):
	"""extracts danceability metric from track-specific json
	"""
	return get_track_features(track_id)["danceability"]

def sort_by_danceability(tracks):
	"""sort by danceability
		not all that useful but a good starting point for more sophisticated algorithm
	""" 
	dance_mapping = {}
	for track in tracks:
		track_id = get_track_id(track)
		dance_mapping[track]=get_danceability(track_id)
	sorted_map = sorted(dance_mapping.items(),key=operator.itemgetter(1)) #list where each element is a tuple of the type (song_name,danceability)
	result = []
	for ele in sorted_map:
		result.append(ele[0]) #list of song names increasing by danceability
	return result



#def sort_filter(track_list):
def getTempo(track_id):
	return get_track_features(track_id)["tempo"]
def getEnergy(track_id):
	return get_track_features(track_id)["energy"]

def tempoSortAsc(track_list):
	order = []
	dict = {}
	rtn =[]
	for track in track_list:
		id = get_track_id(track)
		tempo = getTempo(id)
		order.append(tempo)
		dict[track] = tempo
	order.sort()
	for tempo in order:
		for key in dict:
			if dict[key]==tempo:
				rtn.append(key)
				break
	return rtn

def energySortDesc(track_list):
	order = []
	dict = {}
	rtn =[]
	for song in track_list:
		id = get_track_id(song)
		energy = getEnergy(id)
		order.append(energy)
		dict[song] = energy
	order.sort()
	for energy in order:
		for key in dict:
			if dict[key] == energy:
				rtn.append(key)
				break 
	return rtn[::-1]


def partyList(track_list):
	phase1 = []
	phase2 = []
	phase3 = []
	phase4 = []

	dance_array = sort_by_danceability(track_list)

	length = len(dance_array)
	
	#first order by danceability 
	#Phase 1 --> moderately danceable,
	#Phase 2 --> 25% most danceable 
	#Phase 3 --> moderately danceable to very un-danceable
	for i in range(int(length/4),int(3*length/4)+1):
		if i%2 == 0:
			phase3.append(dance_array[i])
		else:
			phase1.append(dance_array[i])
	for i in range(int(3*length/4)+1,length):
		phase2.append(dance_array[i])

	for i in range(int(length/4)-1,-1,-1): #least danceable at the end of 4
		phase4.append(dance_array[i])
	phase1 = tempoSortAsc(phase1) #phase 1 should increase tempo
	phase2 = phase2[::-1] #phase 2 should increase danceability 

	#account for outliers in phase 2 
	#####################################################
	if (len(phase2) != 0):
		avg2 = 0
		for song in phase2:
			id = get_track_id(song)
			tempo = getTempo(id)
			avg2+= tempo
		avg2/= len(phase2)

	for song in phase2:
		id = get_track_id(song)
		tempo = getTempo(id)
		if tempo-avg2 <= -20:
			#delete item from 2 
			#add item to beginning of 3 
			phase3.insert(0,song)
			phase2.remove(song)
		elif tempo-avg2 >= 20:
			#delete item from 2
			#find a way to smoothen 
			pass
	##########################################################
	phase3 = energySortDesc(phase3)


	#ignore = ['get silly','get silly','get silly','get silly','get silly',]
	ignore = []
	# print(phase1)
	# print(phase2)
	# print(phase3)
	# print(phase4)
	final= phase1+ignore+phase2+ignore+phase3+ignore+phase4
	return final



#tracks = ['alright','false alarm','jumpman','love to lay','low life feat. the weeknd','lucifer','modern soul','pink + white','Señorita','uber everywhere','with them','work feat. drake']
# tracks = ['alright','false alarm','jumpman','love to lay','low life feat. the weeknd','lucifer','modern soul',]
# g= partyList(tracks)
# print(g)
# for track in g:
# 	id = get_track_id(track)
# 	print(get_danceability(id),getTempo(id))
