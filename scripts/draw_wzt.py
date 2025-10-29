from waze_tile_classes import *
import pygame

def main(data: bytes):
    tile = WazeTile(data)

    pygame.init()
    sc = pygame.display.set_mode((1000, 1000))
    pygame.display.set_caption("test")

    colors = {
        3: (255,0,0),
        5: (0,255,0),
        7: (0,0,255),
        15: (255,0,255),
        16: (0,255,255),
        9: (255,255,0)
    }
    for p in tile.polygons: print(p.name, p.cfcc)
    running = True
    while running:
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                running = False
        
        sc.fill((255,255,255))

        for point in tile.points:
            pygame.draw.circle(sc, (0,0,0), (point.x / 15 + 125, 725 - point.y / 15 + 125), 2)

        for line in tile.lines:
            if line.first_point_idx < tile.point_data_count and line.second_point_idx < tile.point_data_count:
                point1 = tile.points[line.first_point_idx]
                point2 = tile.points[line.second_point_idx]
                pygame.draw.line(sc, colors[line.road_type] if line.road_type in colors else (0,0,0), (point1.x / 15 + 125, 725 - point1.y / 15 + 125), (point2.x / 15 + 125, 725 - point2.y / 15 + 125), 3)

        for polygon in tile.polygons:
            if polygon.cfcc == 16:
                continue
            if polygon.cfcc == 15:
                pygame.draw.polygon(sc, (255,0,0), [(p.x / 15 + 125, 725 - p.y / 15 + 125) for p in polygon.points])
            elif polygon.cfcc == 20:
                pygame.draw.polygon(sc, (0,255,255), [(p.x / 15 + 125, 725 - p.y / 15 + 125) for p in polygon.points])
            elif polygon.cfcc == 12:
                pygame.draw.polygon(sc, (0,255,0), [(p.x / 15 + 125, 725 - p.y / 15 + 125) for p in polygon.points])
            else:
                print("unkown polygon type: ", polygon.cfcc)

        pygame.display.flip()

    pygame.quit()

if __name__ == "__main__":
    file_name = input("File: ")

    with open(file_name, "rb") as f:
        data = f.read()

    main(data)