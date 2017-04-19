#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# STL2H by Themistokle "mrt-prodz" Benetatos
# -------------------------------------------
# Convert STL 3D models (ASCII) to header for Tiny 3D Engine
# 
# ------------------------
# http://www.mrt-prodz.com
# http://github.com/mrt-prodz/ATmega328-Tiny-3D-Engine
# -----------------------------------------------------------------------------
import sys, getopt, os

import numpy as np
import stl

# global parameters
param_verbose = False
param_normals = False
param_yes     = False
param_scale   = 1.0

def checkFile(outfile):
	# keep asking user until overwrite is chosen or a non-existing file name is entered
	while (os.path.isfile(outfile)):
		overwrite = raw_input('[!] Output data file "%s" already exists, overwrite? [y/n] ' % outfile)
		if overwrite in ('y', 'Y'):
			return outfile
		elif overwrite in ('n', 'N'):
			outfile = raw_input('[?] Enter new output data file name: ')
			if (outfile == ''):
				outfile = 'temp.h'
	return outfile

def printVerbose(str):
	global param_verbose
	if param_verbose:
		print(str)

def saveDAT(nodes, triangles, outfile, normals = None):
        print('[+] Saving output file:', outfile)
        data  = '// exported with stl2h.py\n'
        data += '// '
        data += ' '.join(sys.argv[:]) + '\n'
        data += '#ifndef MESH_H\n'
        data += '#define MESH_H\n'
        data += '\n'
        data += '#define NODECOUNT ' + str(len(nodes)) + '\n'
        data += '#define TRICOUNT ' + str(len(triangles)) + '\n'
        data += '\n'
        data += '#define NODE(a, b) (long)(pgm_read_dword(&nodes[a][b]))\n'
        data += '#define EDGE(a, b) pgm_read_byte(&faces[a][b])\n'
        data += '#define NORMAL(a, b) (long)(pgm_read_dword(&normals[a][b]))\n'
        data += '\n'
        data += 'const long nodes[NODECOUNT][3] PROGMEM = {\n'
        for index, node in enumerate(nodes):
                data += '  {(long)(' + str(round(float(node[0]), 5)*param_scale) + '*PRES), '\
                         + '(long)(' + str(round(float(node[1]), 5)*param_scale) + '*PRES), '\
                         + '(long)(' + str(round(float(node[2]), 5)*param_scale) + '*PRES)},\n'
        data += '};\n\n'
        data += 'const unsigned char faces[TRICOUNT][3] PROGMEM = {\n'
        for index, face in enumerate(triangles):
                data += '  {' + str(face[0]) + ', ' + str(face[1]) + ', ' + str(face[2]) + '},\n'
        data += '};\n\n'
        data += 'const long normals[TRICOUNT][3] PROGMEM = {\n'
        for index, normal in enumerate(normals):
                data += '  {(long)(' + str(round(float(normal[0]), 5)) + '*PRES), '\
                        + '(long)(' + str(round(float(normal[1]), 5)) + '*PRES), '\
                        + '(long)(' + str(round(float(normal[2]), 5)) + '*PRES)},\n'
        data += '};\n\n'
        data += '#endif // MESH_H\n'
        dat = open(outfile, 'w')
        dat.write(data)
        dat.close()
        printVerbose(data)


def parseSTL(infile, outfile):
        # Using an existing stl file:
        mesh = stl.mesh.Mesh.from_file(infile)
        stlfile = open(infile, 'r')
        nodes = []
        unique_nodes = []
        triangles = []
        if mesh.points.shape[1] != 9:
                print('[!] Error with STL file, each triangle should be made of 3 vertices')
                sys.exit(2)

        nodes = mesh.points.reshape([-1,3])
        # keep only unique nodes
        print('[+] Keeping unique vertices')
        unique_nodes = np.vstack({tuple(row) for row in nodes})
        # output stored vertices
        if param_verbose:
                print(unique_nodes)
        print('[+] Vertices: ', len(unique_nodes))
        
	# assign vertex index to triangle list
        print('[+] Gathering triangles')
        for i in range(mesh.v0.shape[0]):
                i0 = np.argwhere(np.alltrue(unique_nodes == mesh.v0[i],1))[0][0]
                i1 = np.argwhere(np.alltrue(unique_nodes == mesh.v1[i],1))[0][0]
                i2 = np.argwhere(np.alltrue(unique_nodes == mesh.v2[i],1))[0][0]
                triangles.append([i0,i1,i2])
        # print stats
        print('[+] Triangles:', len(triangles))
        saveDAT(unique_nodes, triangles, outfile, mesh.normals)

def usage():
	print('Usage: %s -i <inputfile> -o <outputfile>' % sys.argv[0])
	print('Convert a 3D mesh saved as STL format (ASCII) to header for Tiny 3D Engine.\n')
	print('  -i, --inputfile \t3D mesh in STL file format')
	print('  -o, --outputfile\toutput filename of converted data')
	print('  -s, --scale     \tscale ratio (default 1.0)')
	print('  -n, --normals   \tsave face normals')
	print('  -y, --yes       \tanswer yes to all requests')
	print('  -v, --verbose   \tverbose output')

def main(argv):
	infile = ''
	outfile = ''
	# parse command line arguments
	try:
		opts, args = getopt.getopt(sys.argv[1:],'i:o:s:nhyv',['help', 'yes', 'normals', 'scale=', 'input=', 'output=', 'verbose='])
	except getopt.GetoptError as error:
		print('Error:', str(error))
		usage()
		sys.exit(2)
	if len(opts) < 2:
		usage()
		sys.exit(2)
	for opt, arg in opts:
		if opt in ('-h', '--help'):
			usage()
			sys.exit()
		elif opt in ('-i', '--input'):
			infile = arg
		elif opt in ('-o', '--output'):
			outfile = arg
		elif opt in ('-s', '--scale'):
			global param_scale
			try:
				param_scale = float(arg)
			except Exception as converror:
				print('Error:', str(converror))
				sys.exit(2)
		elif opt in ('-v', '--verbose'):
			global param_verbose
			param_verbose = True
		elif opt in ('-y', '--yes'):
			global param_yes
			param_yes = True
		elif opt in ('-n', '--normals'):
			global param_normals
			param_normals = True
	if (not infile) or (not outfile):
		print('Error: You need to specify both input and output files')
		usage()
		sys.exit(2)
	if infile == outfile:
		print('Error: Input and output files are the same')
		usage()
		sys.exit(2)
	# parse STL file and convert to data
	parseSTL(infile, outfile)

if __name__ == '__main__':
	main(sys.argv[1:])
