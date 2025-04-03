using System.Numerics;
using Silk.NET.Assimp;
using File = System.IO.File;

namespace Preprocessor.Processors;

internal class MeshProcessor : IProcessor
{
    private static bool _assimpIsInitialized;
    private static Assimp? _assetImporter;

    static void Init()
    {
        if (_assimpIsInitialized)
            return;

        _assetImporter = Assimp.GetApi();

        _assimpIsInitialized = true;
    }

    public List<(string, int)> GetValidFileExtensions()
    {
        return [(".glb", 1), (".fbx", 1)];
    }

    public void ImportFile(string filename, string outputFilename)
    {
        Init();
        string meshFile = Path.GetFileName(filename);
        var modifiedOutputPath = Path.ChangeExtension(outputFilename,
            Program.PreprocessorSettings.GetProcessorSetting("MeshExtension", ".mesh"));
        if (File.Exists(modifiedOutputPath))
            File.Delete(modifiedOutputPath);
        Directory.CreateDirectory(Path.GetDirectoryName(modifiedOutputPath)!);

        unsafe
        {
            var scene = _assetImporter!.ImportFile(filename,
                (uint)(PostProcessSteps.Triangulate | PostProcessSteps.OptimizeGraph |
                       PostProcessSteps.OptimizeMeshes));
            if (scene == null)
                throw new Exception($"Failed to import {meshFile} using Assimp.");

            Mesh mesh = new Mesh();

            for (int i = 0; i < scene->MNumMeshes; i++)
            {
                var assimpMesh = scene->MMeshes[i];
                Log.Info($"Adding submesh {assimpMesh->MName} to {modifiedOutputPath}");

                // Get number of indices and vertices
                uint numVertices = assimpMesh->MNumVertices;
                uint numIndices = 0;
                for (int j = 0; j < assimpMesh->MNumFaces; j++)
                {
                    numIndices += assimpMesh->MFaces[j].MNumIndices;
                }

                Log.Info($"Submesh {assimpMesh->MName} has {numVertices} vertices and {numIndices} indices");

                SubMesh subMesh = new(numVertices, numIndices)
                {
                    MaterialIndex = assimpMesh->MMaterialIndex
                };

                // Copy vertices
                for (int vertexIndex = 0; vertexIndex < numVertices; vertexIndex++)
                {
                    // MW @todo: This seems inefficient, but [] operator doesn't seem to work how I expected it to!
                    Vector3* pos = assimpMesh->MVertices + vertexIndex;
                    subMesh.Vertices[vertexIndex].Position = *pos;
                    Vector3* normal = assimpMesh->MNormals + vertexIndex;
                    subMesh.Vertices[vertexIndex].Normal = *normal;
                    Vector3* uv = assimpMesh->MTextureCoords[0] + vertexIndex;
                    subMesh.Vertices[vertexIndex].UV = new Vector2(uv->X, uv->Y);
                }

                // Copy indices
                uint indexOffset = 0;
                for (int faceIndex = 0; faceIndex < assimpMesh->MNumFaces; faceIndex++)
                {
                    var face = assimpMesh->MFaces[faceIndex];
                    for (int index = 0; index < face.MNumIndices; index++)
                    {
                        subMesh.Indices[indexOffset + index] = (ushort)face.MIndices[index];
                    }

                    indexOffset += face.MNumIndices;
                }

                mesh.Submeshes.Add(subMesh);
            }

            _assetImporter.FreeScene(scene);

            mesh.Export(modifiedOutputPath);

            Log.Trace($"{filename} -> {modifiedOutputPath}");
        }
    }
}
