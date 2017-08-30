import records
import requests
import operator 

db = records.Database('mysql://student:6s08student@localhost/iesc')

def web_handler(request):
	method = request['REQUEST_METHOD']
	if method == 'GET':
		parameters = request['GET']
		if 'Song_names' in parameters:
			song_names = parameters['Song_names']
			song_names = song_names.split('*')
			for i in range(len(song_names)):
				song_names[i]=song_names[i].lower()
			return db.query("SELECT DISTINCT name FROM song_info").as_dict()
			return partyList(song_names)
		else:
			return 'not given a list of songs'

	elif method == "POST":
		parameters = request['POST']
		if 'Song_names' in parameters and 'post' in parameters:
			song_names = parameters['Song_names']
			song_names = song_names.split('*')
			for song in song_names:
				id_num = get_track_id(song)
				db.query("INSERT INTO song_info (name, track_id, danceability, tempo,energy) VALUES (:name, :id,:dance,:tempo,:energy)",name=song,id=id_num,dance=get_danceability(id_num),tempo=get_tempo(id_num),energy=get_energy(id_num))
			return "Inserted into database!"

		if 'Song_names' in parameters and 'get' in parameters:
			song_names = parameters['Song_names']
			song_names = song_names.split('*')
			temp = []
			for i in song_names:
				if '\r' in i:
					temp.append(i[:-1])
				else:
					temp.append(i)
			return partyList(temp)
		else:
			return 'not given a list of songs'



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
	token = 'BQA-8-IycppptsrE-hG3-3qCB1mXiIpxg_o7ceR_T0sT8rgdtMHmwtOlFcFyA8yJDTSO9YH_dUdLskVX6nneFydkmLxvE7Dw3LgM3H5Rxk9kRdxkCMH1ArPthvhsBecVDF9tYQMNS3o'
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
	query = "SELECT DISTINCT name,danceability FROM song_info WHERE name in (" + ','.join(("'"+track+"'" for track in tracks))+")"
	rows = db.query(query)
	dict_rows = rows.as_dict()#Need to fix by creating dictionary 
	sorted_map = sorted(dict_rows,key=operator.itemgetter('danceability')) #list where each element is a tuple of the type (song_name,danceability)
	result = []
	for ele in sorted_map:
		result.append(ele['name']) #list of song names increasing by danceability
	return result



#def sort_filter(track_list):
def get_tempo(track_id):
	return get_track_features(track_id)["tempo"]
def get_energy(track_id):
	return get_track_features(track_id)["energy"]

def tempoSortAsc(track_list):
	order = []
	rtn =[]
	mapping = {}
	query = "SELECT DISTINCT name,tempo FROM song_info WHERE name in (" + ','.join(("'"+track+"'" for track in track_list))+")"
	rows = db.query(query)
	dict_rows = rows.as_dict()#Need to fix by creating dictionary 

	for ele in dict_rows:
		order.append(ele['tempo'])
		mapping[ele['name']] = ele['tempo']
	order.sort()
	for tempo in order:
		for key in mapping:
			if mapping[key]==tempo:
				rtn.append(key)
				break
	return rtn

def energySortDesc(track_list):
	order = []
	rtn =[]
	mapping = {}
	query = "SELECT DISTINCT name,energy FROM song_info WHERE name in (" + ','.join(("'"+track+"'" for track in track_list))+")"
	rows = db.query(query)
	dict_rows = rows.as_dict()#Need to fix by creating dictionary 
	for ele in dict_rows:
		order.append(ele['energy'])
		mapping[ele['name']] = ele['energy']
	order.sort()
	for energy in order:
		for key in mapping:
			if mapping[key] == energy:
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
	avg2 = 0
	query = "SELECT DISTINCT name,tempo FROM song_info WHERE name in (" + ','.join(("'"+track+"'" for track in phase2))+")"
	rows = db.query(query)
	dict_rows = rows.as_dict()#Need to fix by creating dictionary 
	for ele in dict_rows:
		avg2+=ele['tempo'] #might be something like this still not entirely sure how our dictionary will turn out, waiting on database update 

	avg2/= len(phase2)

	for song in dict_rows:
		if song['tempo']-avg2 <= -20:
			#delete item from 2 
			#add item to beginning of 3 
			phase3.insert(0,song)
			phase2.remove(song)    					##Need to edit phase2 based on dict_rows, not implemented yet
		elif song['tempo']-avg2 >= 20:
			#delete item from 2
			#find a way to smoothen 
			pass
	##########################################################
	phase3 = energySortDesc(phase3)

	ignore = []
	final= phase1+ignore+phase2+ignore+phase3+ignore+phase4
	return final

