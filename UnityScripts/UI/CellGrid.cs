using UnityEngine;

public class CellGrid : MonoBehaviour
{
    public Material lineMaterial;

    public float lineZ = -1f;

    void Start()
    {
        DrawCellGrid();
    }

    void DrawCellGrid()
    {
        // Zone 시작 좌표
        float[] zoneStarts = { -5f, 95f, 195f };
        float cellSize = 22f;      // 각 Zone 내 Cell 크기 (고정)
        int cellsPerZone = 5;

        for (int zX = 0; zX < zoneStarts.Length; zX++)
        {
            for (int zY = 0; zY < zoneStarts.Length; zY++)
            {
                float zoneStartX = zoneStarts[zX];
                float zoneEndX = zoneStartX + cellsPerZone * cellSize;

                float zoneStartY = zoneStarts[zY];
                float zoneEndY = zoneStartY + cellsPerZone * cellSize;

                // Vertical lines
                for (int i = 0; i <= cellsPerZone; i++)
                {
                    float x = zoneStartX + i * cellSize;
                    CreateLine(new Vector3(x, zoneStartY, lineZ), new Vector3(x, zoneEndY, lineZ));
                }

                // Horizontal lines
                for (int j = 0; j <= cellsPerZone; j++)
                {
                    float y = zoneStartY + j * cellSize;
                    CreateLine(new Vector3(zoneStartX, y, lineZ), new Vector3(zoneEndX, y, lineZ));
                }
            }
        }
    }

    void CreateLine(Vector3 start, Vector3 end)
    {
        GameObject go = new GameObject("GridLine");
        go.transform.parent = transform;

        LineRenderer lr = go.AddComponent<LineRenderer>();
        lr.material = lineMaterial;
        lr.startWidth = 0.05f;
        lr.endWidth = 0.05f;
        lr.positionCount = 2;
        lr.SetPosition(0, start);
        lr.SetPosition(1, end);
        lr.sortingOrder = 999;
    }
}
