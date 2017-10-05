import sys
import json
import os.path

def lutvalue(board, idx, name):
    return str(data["SX1301_array_conf"][board]["rf_chain_conf"][rf]["tx_lut"][idx][name])

# check if file exists
if os.path.isfile(sys.argv[1]) == False:
    sys.stderr.write("File " + sys.argv[1] + " doesn't exist !\n")
    sys.exit(1)

# load json file
with open(sys.argv[1]) as data_file:
    try:
        data = json.load(data_file)
    except Exception:
        sys.stderr.write("JSON error while reading file " + sys.argv[1] + "\n")
        sys.exit(1)

nbboard=len(data["SX1301_array_conf"])
# generate board section
for b in range(0, nbboard):
	print '[board:' + str(b) + ']'
	try:
		print '\troomtemp=' + str(data["SX1301_array_conf"][b]["calibration_temperature_celsius_room"])
		print '\tad9361temp=' + str(data["SX1301_array_conf"][b]["calibration_temperature_code_ad9361"])
	except:
		pass
	print

# generate rfconf section
for b in range(0, nbboard):
	for rf in range(0,2):
	    print '[rfconf:' + str(b) + ':' + str(rf) + ']'
	    if "rssi_offset" not in data["SX1301_array_conf"][b]["rf_chain_conf"][rf]:
		sys.stderr.write("JSON error while reading file " + sys.argv[1] + ", no 'rssi_offset' found. Abort\n")
		sys.exit(1)
	    print '\trssioffset=' + str(data["SX1301_array_conf"][b]["rf_chain_conf"][rf]["rssi_offset"])
	    if "dsp_rssi_offset" in data["SX1301_array_conf"][b]["rf_chain_conf"][rf]:
		print '\tdsprssioffset=' + str(data["SX1301_array_conf"][b]["rf_chain_conf"][rf]["dsp_rssi_offset"])
	    try:
	    	print '\trssioffsetcoeffa=' + str(data["SX1301_array_conf"][b]["rf_chain_conf"][rf]["rssi_offset_coeff_a"])
	    	print '\trssioffsetcoeffb=' + str(data["SX1301_array_conf"][b]["rf_chain_conf"][rf]["rssi_offset_coeff_b"])
	    except:
	    	pass
	    print

# generate lut section
for b in range(0, nbboard):
	for rf in range(0,2):
	    print '[lut/' + str(b) + '/' + str(rf) + ']'
	    print ';\tidx\t=\tpower\tdig\tatten\tdacvref\tdacword\tcoef_a\tcoef_b'
	    for idx in range(0,32):
		print '\t' + str(idx) + "\t=\t" + lutvalue(b, idx, "rf_power") + "\t" + lutvalue(b, idx, "fpga_dig_gain") + \
		    "\t" + lutvalue(b, idx, "ad9361_atten") + "\t" + lutvalue(b, idx, "ad9361_auxdac_vref") + \
		    "\t" + lutvalue(b, idx, "ad9361_auxdac_word") + "\t" + lutvalue(b, idx, "ad9361_tcomp_coeff_a") + \
		    "\t" + lutvalue(b, idx, "ad9361_tcomp_coeff_b")
	    print
