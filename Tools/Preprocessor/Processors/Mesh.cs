using System.Numerics;
using System.Runtime.InteropServices;

namespace Preprocessor.Processors;

public struct MeshVertex
{
    public Vector3 Position;
    public Vector3 Normal;
    public Vector2 UV;
}

public class SubMesh(uint numVertices, uint numIndices)
{
    public MeshVertex[] Vertices = new MeshVertex[numVertices];
    public ushort[] Indices = new ushort[numIndices];
    public uint MaterialIndex;
    public uint NumVerts = numVertices, NumIndices = numIndices;
}

public class Mesh
{
    public List<SubMesh> Submeshes = new();

    public void Export(string filename)
    {
        FileStream fileStream = new(filename, FileMode.Create);
        using BinaryWriter writer = new(fileStream);

        writer.Write(0x53574150); // Magic number "PAWS"
        writer.Write(Submeshes.Count);

        foreach (var submesh in Submeshes)
        {
            writer.Write(submesh.NumVerts);
            writer.Write(submesh.NumIndices);
            writer.Write(submesh.MaterialIndex);

            int vertexSize = Marshal.SizeOf<MeshVertex>();
            byte[] vertexData = new byte[submesh.NumVerts * vertexSize];
            GCHandle handle = GCHandle.Alloc(submesh.Vertices, GCHandleType.Pinned);
            Marshal.Copy(handle.AddrOfPinnedObject(), vertexData, 0, vertexData.Length);
            handle.Free();
            writer.Write(vertexData);

            byte[] indexData = new byte[submesh.NumIndices * sizeof(ushort)];
            handle = GCHandle.Alloc(submesh.Indices, GCHandleType.Pinned);
            Marshal.Copy(handle.AddrOfPinnedObject(), indexData, 0, indexData.Length);
            handle.Free();
            writer.Write(indexData);
        }
    }
}