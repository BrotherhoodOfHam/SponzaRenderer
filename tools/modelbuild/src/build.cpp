/*
	Model parser tool
*/

#include <tsconfig.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

//Engine headers
#include <tscore/filesystem/path.h>
#include <tscore/system/time.h>
//Model format types and constants
#include <tsgraphics/model/modeldefs.h>

//Assimp headers
#include "assimp\Importer.hpp"
#include "assimp\material.h"
#include "assimp\mesh.h"
#include "assimp\postprocess.h"
#include "assimp\scene.h"
#include "assimp\Logger.hpp"
#include "assimp\DefaultLogger.hpp"

using namespace std;
using namespace ts;

///////////////////////////////////////////////////////////////////////////////////////////////
//Helper functions
///////////////////////////////////////////////////////////////////////////////////////////////

//If g_quiet is true then no output is printed to the console
bool g_quiet = false;

template<typename ... t>
void printline(const char* str, t ... args)
{
	if (!g_quiet)
	{
		printf(format(str, args...).c_str());
		printf("\n");
	}
}

template<typename ... t>
void printerror(const char* str, t ... args)
{
	if (!g_quiet)
	{
		printf("ERROR: ");
		printf(format(str, args...).c_str());
		printf("\n");
	}
}

template<typename ... t>
void printwarning(const char* str, t ... args)
{
	if (!g_quiet)
	{
		printf("WARNING: ");
		printf(format(str, args...).c_str());
		printf("\n");
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////

bool exportModel(const aiScene* model, ostream& outputstream);
bool exportMaterials(const aiScene* scene, ostream& outputstream);

void attachAILogger(bool verbose);

//Command line flags
enum CMD_FLAGS
{
	CmdNone			= 0,
	CmdGenMaterials	= 1,
	CmdQuiet		= 2,
	CmdLogVerbose	= 4,
	CmdHasTarget	= 8,
	CmdHasOutput	= 16
};

//Command line errors
enum CMD_ERROR
{
	CmdErrOk		  = 0,
	CmdErrFail		  = 1,
	CmdErrInvalidArgC = 2
};

//Parses command line arguments - and retrieves program parameters
int parseCmdArgs(int argc, char** argv, int& flags, string& target, string& output);

///////////////////////////////////////////////////////////////////////////////////////////////
//Main function
///////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
	if (argc <= 1)
	{
		printf("This application was called with invalid arguments, arguments are:\n");
		printf("[TARGET]  -t : path to model file to parse\n");
		printf("[OUTPUT]  -o : path to directory you want to produce the output model\n");
		printf("[QUIET]   -q : disable printing to stdout\n");
		printf("[MAT]     -m : generate a material file\n");
		printf("[VERBOSE] -v : verbose logging\n");
		
		return EXIT_FAILURE;
	}
	
	int flagArgs = 0;
	string targetArg;
	string outputArg;

	if (int ret = parseCmdArgs(argc, argv, flagArgs, targetArg, outputArg))
	{
		return ret;
	}
	
	bool has_target = (flagArgs & CmdHasTarget) != 0; //A path to a model file
	bool has_output = (flagArgs & CmdHasOutput) != 0; //An output directory
	bool has_mat = (flagArgs & CmdGenMaterials) != 0; //Generate a material file for this model
	g_quiet = (flagArgs & CmdQuiet) != 0;

	Path targetpath;
	Path outputpath;

	if (has_target)
	{
		targetpath = Path(targetArg.c_str());
		if (targetpath.str() == "")
		{
			printerror("An invalid target path was specified");
			return EXIT_FAILURE;
		}
	}
	else
	{
		printerror("A target path must be specified");
		return EXIT_FAILURE;
	}

	if (has_output)
	{
		outputpath = Path(outputArg.c_str());
	}
	else
	{
		//If no output path is specified then the default output path is the target path
		outputpath = targetpath.getParent();
	}

	Assimp::Importer imp;

	//Attach an Assimp logger
	attachAILogger((flagArgs & CmdLogVerbose) != 0);

	printline("parsing model \"%\"...", targetpath.str());
	
	Stopwatch t;
	t.start();

	const aiScene* scene = imp.ReadFile(targetpath.str(),
		aiProcess_CalcTangentSpace |
		aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_OptimizeMeshes |
		//aiProcess_GenNormals |
		aiProcess_SplitLargeMeshes |
		aiProcess_ConvertToLeftHanded |
		//aiProcess_SortByPType |
		aiProcess_PreTransformVertices
	);

	t.stop();
	double dt = t.deltaTime();

	if (scene)
	{
		printline("model parsed successfully (%ms)", dt);
		
		Path modelfilepath(outputpath);
		Path matrlfilepath(outputpath);
		
		//Extract filename from target
		string filename = targetpath.getDirectoryTop().str();
		filename.erase(filename.find_last_of('.'), string::npos);
		
		//Append new file extension
		modelfilepath.addDirectories(filename + ".tsm");
		matrlfilepath.addDirectories(filename + ".tmat");
		
		printline("writing model data...");
		
		//Open model file stream
		ofstream modelfile(modelfilepath.str(), ios::binary);
		
		//Export Assimp model data to custom format
		if (exportModel(scene, modelfile))
		{
			printline("model data written successfully");
		}
		else
		{
			printerror("failed to write model data");
			return EXIT_FAILURE;
		}
		
		//Export materials
		if (has_mat)
		{
			printline("writing materials...");
			
			ofstream matfile(matrlfilepath.str());
			
			if (exportMaterials(scene, matfile))
			{
				printline("materials written successfully");
			}
			else
			{
				printwarning("failed to write materials");
				return EXIT_FAILURE;
			}
		}
	}
	else
	{
		printerror(imp.GetErrorString());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////////////////
//Export functions
///////////////////////////////////////////////////////////////////////////////////////////////

bool exportModel(const aiScene* scene, ostream& outputstream)
{
	SModelHeader header;
	header.numMeshes = scene->mNumMeshes;
	
	vector<SModelMesh> meshes;
	vector<SModelVertex> vertices;
	vector<ModelIndex> indices;
	
	meshes.reserve(header.numMeshes);
	
	for (uint i = 0; i < header.numMeshes; i++)
	{
		SModelMesh mesh;
		
		aiMesh* aimesh = scene->mMeshes[i];
		
		aiString materialName;
		scene->mMaterials[aimesh->mMaterialIndex]->Get(AI_MATKEY_NAME, materialName);
		
		if (strlen(materialName.C_Str()) > MaxMaterialNameLength)
		{
			printwarning("the material name must be shorter than % chars", MaxMaterialNameLength);
		}
		
		mesh.materialName.set(materialName.C_Str());
		
		printline("reading mesh - material '%'", materialName.C_Str());
		
		//Vertex attribute mask
		uint8 attribs = 0;
		
		for (uint j = 0; j < aimesh->mNumVertices; j++)
		{
			SModelVertex vertex;

			if (aimesh->HasPositions())
			{
				vertex.position.x() = aimesh->mVertices[j].x;
				vertex.position.y() = aimesh->mVertices[j].y;
				vertex.position.z() = aimesh->mVertices[j].z;
				vertex.position.w() = 1.0f;

				attribs |= eModelVertexAttributePosition;
			}

			if (aimesh->HasNormals())
			{
				vertex.normal.x() = aimesh->mNormals[j].x;
				vertex.normal.y() = aimesh->mNormals[j].y;
				vertex.normal.z() = aimesh->mNormals[j].z;
				vertex.normal.normalize();

				attribs |= eModelVertexAttributeNormal;
			}

			if (aimesh->HasVertexColors(0))
			{
				vertex.colour.x() = aimesh->mColors[0][j].r;
				vertex.colour.y() = aimesh->mColors[0][j].g;
				vertex.colour.z() = aimesh->mColors[0][j].b;
				vertex.colour.w() = aimesh->mColors[0][j].a;

				attribs |= eModelVertexAttributeColour;
			}

			if (aimesh->HasTextureCoords(0))
			{
				vertex.texcoord.x() = aimesh->mTextureCoords[0][j].x;
				vertex.texcoord.y() = aimesh->mTextureCoords[0][j].y;

				attribs |= eModelVertexAttributeTexcoord;
			}

			if (aimesh->HasTangentsAndBitangents())
			{
				vertex.tangent.x() = aimesh->mTangents[j].x;
				vertex.tangent.y() = aimesh->mTangents[j].y;
				vertex.tangent.z() = aimesh->mTangents[j].z;
				vertex.tangent.normalize();

				attribs |= eModelVertexAttributeTangent;

				vertex.bitangent.x() = aimesh->mBitangents[j].x;
				vertex.bitangent.y() = aimesh->mBitangents[j].y;
				vertex.bitangent.z() = aimesh->mBitangents[j].z;
				vertex.bitangent.normalize();

				attribs |= eModelVertexAttributeBitangent;
			}
			
			vertices.push_back(vertex);
		}
		
		ModelIndex indexcount = 0;
		ModelIndex indexoffset = (ModelIndex)indices.size();
		
		for (UINT c = 0; c < aimesh->mNumFaces; c++)
		{
			for (UINT e = 0; e < aimesh->mFaces[c].mNumIndices; e++)
			{
				indices.push_back(aimesh->mFaces[c].mIndices[e]);
				indexcount++;
			}
		}
		
		mesh.indexOffset = indexoffset;
		mesh.indexCount = indexcount;
		mesh.numVertices = aimesh->mNumVertices;
		mesh.vertexAttributeMask = attribs;
		
		meshes.push_back(mesh);
	}
	
	header.numVertices = (ModelIndex)vertices.size();
	header.numIndices = (ModelIndex)indices.size();
	
	outputstream.write(reinterpret_cast<const char*>(&header), sizeof(SModelHeader));
	outputstream.write(reinterpret_cast<const char*>(&meshes[0]), sizeof(SModelMesh) * meshes.size());
	outputstream.write(reinterpret_cast<const char*>(&vertices[0]), sizeof(SModelVertex) * vertices.size());
	outputstream.write(reinterpret_cast<const char*>(&indices[0]), sizeof(ModelIndex) * indices.size());
	
	printline("Num vertices = %", vertices.size());
	printline("Num indices = %", indices.size());
	printline("Num meshes = %", meshes.size());
	
	return outputstream.good();
}

void writeColourToStream(ostream& stream, const aiColor3D& colour)
{
	stream << colour.r << ", " << colour.g << ", " << colour.b;
}

bool exportMaterials(const aiScene* scene, ostream& outputstream)
{
	outputstream << "#\n";
	outputstream << "#	Material file\n";
	outputstream << "#\n\n";

	printline("num materials %", scene->mNumMaterials);
	
	if (scene->HasMaterials())// || !no_materials)
	{
		for (uint32 i = 0; i < scene->mNumMaterials; i++)
		{
			aiMaterial* aimaterial = scene->mMaterials[i];
			
			aiString aimatName;
			aimaterial->Get(AI_MATKEY_NAME, aimatName);
			printline("writing material: %", aimatName.C_Str());
			
			outputstream << "[" << aimatName.C_Str() << "]\n";
			
			//Set colour properties
			aiColor3D colour;

			aimaterial->Get(AI_MATKEY_COLOR_DIFFUSE, colour);
			outputstream << "diffuseColour = ";
			writeColourToStream(outputstream, colour);
			outputstream << "\n";

			aimaterial->Get(AI_MATKEY_COLOR_AMBIENT, colour);
			outputstream << "ambientColour = ";
			writeColourToStream(outputstream, colour);
			outputstream << "\n";
			
			aimaterial->Get(AI_MATKEY_COLOR_EMISSIVE, colour);
			outputstream << "emissiveColour = ";
			writeColourToStream(outputstream, colour);
			outputstream << "\n";
			
			aimaterial->Get(AI_MATKEY_COLOR_SPECULAR, colour);
			outputstream << "specularColour = ";
			writeColourToStream(outputstream, colour);
			outputstream << "\n";
			
			float shininess = 0.0f;
			float alpha = 0.0f;
			aimaterial->Get(AI_MATKEY_SHININESS, shininess);
			outputstream << "shininess = " << shininess << "\n";
			aimaterial->Get(AI_MATKEY_OPACITY, alpha);
			outputstream << "alpha = " << alpha << "\n";
			
			//Set texture path properties
			aiString tex;
			Path texpath;

			tex.Clear();
			aimaterial->GetTexture(aiTextureType::aiTextureType_DIFFUSE, 0, &tex);
			texpath.composePath(tex.C_Str());
			if ((string)texpath.str() != "")
				outputstream << "diffuseMap = " << texpath.str() << endl;

			tex.Clear();
			aimaterial->GetTexture(aiTextureType::aiTextureType_SPECULAR, 0, &tex);
			texpath.composePath(tex.C_Str());
			if ((string)texpath.str() != "")
				outputstream << "specularMap = " << texpath.str() << endl;

			tex.Clear();
			aimaterial->GetTexture(aiTextureType::aiTextureType_AMBIENT, 0, &tex);
			texpath.composePath(tex.C_Str());
			if ((string)texpath.str() != "")
				outputstream << "ambientMap = " << texpath.str() << endl;

			tex.Clear();
			aimaterial->GetTexture(aiTextureType::aiTextureType_DISPLACEMENT, 0, &tex);
			texpath.composePath(tex.C_Str());
			if ((string)texpath.str() != "")
				outputstream << "displacementMap = " << texpath.str() << endl;

			tex.Clear();
			aimaterial->GetTexture(aiTextureType::aiTextureType_NORMALS, 0, &tex);
			texpath.composePath(tex.C_Str());
			if ((string)texpath.str() != "")
				outputstream << "normalMap = " << texpath.str() << endl;

			outputstream << endl;
		}
	}
	else
	{
		printwarning("No materials could be found");
		return false;
	}
	
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////

int parseCmdArgs(int argc, char** argv, int& flags, string& target, string& output)
{
	if (argc == 1)
		return CmdErrInvalidArgC;

	string argToken;
	char prevToken = 0;

	for (int i = 1; i < argc; i++)
	{
		argToken = trim(argv[i]);
		
		if (argToken.size() > 1)
		{
			//If first character is '-' then the current argument token is a tag
			if (argToken[0] == '-')
			{
				//Set flags
				switch (argToken[1])
				{
					case 't':
					{
						flags |= CmdHasTarget;
						break;
					}
					case 'o':
					{
						flags |= CmdHasOutput;
						break;
					}
					case 'q':
					{
						flags |= CmdQuiet;
						break;
					}
					case 'm':
					{
						flags |= CmdGenMaterials;
						break;
					}
					case 'v':
					{
						flags |= CmdLogVerbose;
						break;
					}
					default:
					{
						printwarning("Unknown argument tag '%'", argToken[1]);
					}
				}

				prevToken = argToken[1];
			}
			else
			{
				if (prevToken == 't')
				{
					//Current token must be the argument for -t
					target = argToken;
				}
				else if (prevToken == 'o')
				{
					//Current token must be the argument for -o
					output = argToken;
				}
				else
				{
					printwarning("Argument tag '%' does not expect a parameter: \"%\"", prevToken, argToken);
				}
			}
		}
	}

	return CmdErrOk;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void attachAILogger(bool verbose)
{
	if (!g_quiet)
	{
		Assimp::Logger::LogSeverity severity = (verbose) ? Assimp::Logger::VERBOSE : Assimp::Logger::NORMAL;

		// Create a logger instance for Console Output
		Assimp::DefaultLogger::create("", severity, aiDefaultLogStream_STDOUT);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////