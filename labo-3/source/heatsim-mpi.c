#include <assert.h>
#include <stddef.h>

#include "heatsim.h"
#include "log.h"

MPI_Datatype struct_create(grid_t* grid) {
    int block_lengths[1] = {grid->width_padded * grid->height_padded};
    MPI_Aint displacements[1] = {0};

    MPI_Datatype types[1] = {MPI_DOUBLE};

    MPI_Datatype grid_struct;
    MPI_Type_create_struct(1, block_lengths, displacements, types, &grid_struct);
    MPI_Type_commit(&grid_struct);

    return grid_struct;
}

int heatsim_init(heatsim_t* heatsim, unsigned int dim_x, unsigned int dim_y) {
    /*
     * TODO: Initialiser tous les membres de la structure `heatsim`.
     *       Le communicateur doit être périodique. Le communicateur
     *       cartésien est périodique en X et Y.
     */

    int error = MPI_SUCCESS;

    int dims[2] = {dim_x, dim_y};
    int periods[2] = {1, 1};
    error = MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &heatsim->communicator);
    if (error != MPI_SUCCESS)
    {
        LOG_ERROR_MPI("Failed to create cart", error);
        goto fail_exit;
    }

    error = MPI_Comm_size(MPI_COMM_WORLD, &heatsim->rank_count);

    if (error != MPI_SUCCESS)
    {
        LOG_ERROR_MPI("Failed to get comm size", error);
        goto fail_exit;
    }

    error = MPI_Comm_rank(MPI_COMM_WORLD, &heatsim->rank);

    if (error != MPI_SUCCESS)
    {
        LOG_ERROR_MPI("Failed to get the rank", error);
        goto fail_exit;
    }

    error = MPI_Cart_shift(heatsim->communicator, 1, 1, &heatsim->rank_north_peer, &heatsim->rank_south_peer);

    if (error != MPI_SUCCESS)
    {
        LOG_ERROR_MPI("Failed to get north and south peer", error);
        goto fail_exit;
    }

    error = MPI_Cart_shift(heatsim->communicator, 0, 1, &heatsim->rank_west_peer, &heatsim->rank_east_peer);

    if (error != MPI_SUCCESS)
    {
        LOG_ERROR_MPI("Failed to get east and west peer", error);
        goto fail_exit;
    }

    error = MPI_Cart_coords(heatsim->communicator, heatsim->rank, 2, heatsim->coordinates);

    if (error != MPI_SUCCESS)
    {
        LOG_ERROR_MPI("Failed to get coordinates", error);
        goto fail_exit;
    }

    if (heatsim->communicator == MPI_COMM_NULL) {
        goto fail_exit;
    }

    return 0;

fail_exit:
    return -1;
}

int heatsim_send_grids(heatsim_t* heatsim, cart2d_t* cart) {
    /*
     * TODO: Envoyer toutes les `grid` aux autres rangs. Cette fonction
     *       est appelé pour le rang 0. Par exemple, si le rang 3 est à la
     *       coordonnée cartésienne (0, 2), alors on y envoit le `grid`
     *       à la position (0, 2) dans `cart`.
     *
     *       Il est recommandé d'envoyer les paramètres `width`, `height`
     *       et `padding` avant les données. De cette manière, le receveur
     *       peut allouer la structure avec `grid_create` directement.
     *
     *       Utilisez `cart2d_get_grid` pour obtenir la `grid` à une coordonnée.
     */

    for (unsigned int i = 1; i < heatsim->rank_count; i++) {

        int coordinates[2];
        MPI_Cart_coords(heatsim->communicator, i, 2, coordinates);
        grid_t* grid = cart2d_get_grid(cart, coordinates[0], coordinates[1]);
        if (grid == NULL) {
            goto fail_exit;
        }

        int error = MPI_SUCCESS;

        // Envoie les données de la grille
        error = MPI_Send(&grid->width, 3, MPI_UNSIGNED, i, 0, heatsim->communicator);

        if (error != MPI_SUCCESS)
        {
            LOG_ERROR_MPI("Failed send grid parameters", error);
            goto fail_exit;
        }

        MPI_Datatype grid_struct = struct_create(grid);

        error = MPI_Send(grid->data, 1, grid_struct, i, 1, heatsim->communicator);

        if (error != MPI_SUCCESS)
        {
            LOG_ERROR_MPI("Failed send grid data", error);
            goto fail_exit;
        }

        MPI_Type_free(&grid_struct);
    }


    return 0;

fail_exit:
    return -1;
}

grid_t* heatsim_receive_grid(heatsim_t* heatsim) {
    /*
     * TODO: Recevoir un `grid ` du rang 0. Il est important de noté que
     *       toutes les `grid` ne sont pas nécessairement de la même
     *       dimension (habituellement ±1 en largeur et hauteur). Utilisez
     *       la fonction `grid_create` pour allouer un `grid`.
     *
     *       Utilisez `grid_create` pour allouer le `grid` à retourner.
     */

    typedef struct grid_params{
        unsigned int width;
        unsigned int height;
        unsigned int padding;
    } grid_params_t;

    grid_params_t params;


    int error = MPI_SUCCESS;

    error = MPI_Recv(&params, 3, MPI_UNSIGNED, 0, 0, heatsim->communicator, MPI_STATUS_IGNORE);

    if (error != MPI_SUCCESS)
    {
        LOG_ERROR_MPI("Failed receive grid parameters", error);
        goto fail_exit;
    }
    
    grid_t* grid = grid_create(params.width, params.height, params.padding);

    if (grid == NULL) {
        goto fail_exit;
    }

    MPI_Datatype grid_struct = struct_create(grid);

    error = MPI_Recv(grid->data, 1, grid_struct, 0, 1, heatsim->communicator, MPI_STATUS_IGNORE);

    if (error != MPI_SUCCESS)
    {
        LOG_ERROR_MPI("Failed receive grid data", error);
        goto fail_exit;
    }
    
    MPI_Type_free(&grid_struct);

    return grid;

fail_exit:
    return NULL;
}

int heatsim_exchange_borders(heatsim_t* heatsim, grid_t* grid) {
    assert(grid->padding == 1);

    /*
     * TODO: Échange les bordures de `grid`, excluant le rembourrage, dans le
     *       rembourrage du voisin de ce rang. Par exemple, soit la `grid`
     *       4x4 suivante,
     *
     *                            +-------------+
     *                            | x x x x x x |
     *                            | x A B C D x |
     *                            | x E F G H x |
     *                            | x I J K L x |
     *                            | x M N O P x |
     *                            | x x x x x x |
     *                            +-------------+
     *
     *       où `x` est le rembourrage (padding = 1). Ce rang devrait envoyer
     *
     *        - la bordure [A B C D] au rang nord,
     *        - la bordure [M N O P] au rang sud,
     *        - la bordure [A E I M] au rang ouest et
     *        - la bordure [D H L P] au rang est.
     *
     *       Ce rang devrait aussi recevoir dans son rembourrage
     *
     *        - la bordure [A B C D] du rang sud,
     *        - la bordure [M N O P] du rang nord,
     *        - la bordure [A E I M] du rang est et
     *        - la bordure [D H L P] du rang ouest.
     *
     *       Après l'échange, le `grid` devrait avoir ces données dans son
     *       rembourrage provenant des voisins:
     *
     *                            +-------------+
     *                            | x m n o p x |
     *                            | d A B C D a |
     *                            | h E F G H e |
     *                            | l I J K L i |
     *                            | p M N O P m |
     *                            | x a b c d x |
     *                            +-------------+
     *
     *       Utilisez `grid_get_cell` pour obtenir un pointeur vers une cellule.
     */

    MPI_Datatype vector; 
    int error = MPI_SUCCESS;

    error = MPI_Type_vector(grid->height, 1, grid->width_padded, MPI_DOUBLE, &vector);
    
    if (error != MPI_SUCCESS)
    {
        LOG_ERROR_MPI("Failed function MPI_Type_vector", error);
        goto fail_exit;
    }

    MPI_Type_commit(&vector);

    double* first_cell = grid_get_cell(grid, 0, 0);
    double* last_row = grid_get_cell(grid, 0, grid->height - 1);
    double* last_column = grid_get_cell(grid, grid->width - 1, 0);
    double* north_border = grid_get_cell(grid, 0, -1);
    double* west_border = grid_get_cell(grid, -1, 0);
    double* south_border = grid_get_cell(grid, 0, grid->height);
    double* east_border = grid_get_cell(grid, grid->width, 0);
    
    if (heatsim->rank_west_peer != heatsim->rank) {
        if (heatsim->coordinates[0] % 2 == 0) {

            // Send EAST border
            error = MPI_Send(last_column, 1, vector, heatsim->rank_east_peer, 2, heatsim->communicator);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed send east border", error);
                goto fail_exit;
            }

            // And receive WEST
            error = MPI_Recv(west_border, 1, vector, heatsim->rank_west_peer, 2, heatsim->communicator, MPI_STATUS_IGNORE);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed receive west border", error);
                goto fail_exit;
            }

            // Then send WEST
            error = MPI_Send(first_cell, 1, vector, heatsim->rank_west_peer, 3, heatsim->communicator);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed send west border", error);
                goto fail_exit;
            }

            // Then wait for EAST border
            error = MPI_Recv(east_border, 1, vector, heatsim->rank_east_peer, 3, heatsim->communicator, MPI_STATUS_IGNORE);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed receive east border", error);
                goto fail_exit;
            }

        } else {

            // Receive WEST
            error = MPI_Recv(west_border, 1, vector, heatsim->rank_west_peer, 2, heatsim->communicator, MPI_STATUS_IGNORE);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed receive west border", error);
                goto fail_exit;
            }

            // Then send EAST border
            error = MPI_Send(last_column, 1, vector, heatsim->rank_east_peer, 2, heatsim->communicator);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed send east border", error);
                goto fail_exit;
            }

            // Then wait for EAST border
            error = MPI_Recv(east_border, 1, vector, heatsim->rank_east_peer, 3, heatsim->communicator, MPI_STATUS_IGNORE);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed receive east border", error);
                goto fail_exit;
            }

            // Then send WEST
            error = MPI_Send(first_cell, 1, vector, heatsim->rank_west_peer, 3, heatsim->communicator);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed send west border", error);
                goto fail_exit;
            }
        }
    } else {
        for (int i = 0; i < grid->height; i++) {
            west_border[i * grid->width_padded] = last_column[i * grid->width_padded];
            east_border[i * grid->width_padded] = first_cell[i * grid->width_padded];
        }
    }

    if (heatsim->rank_north_peer != heatsim->rank) {
        if (heatsim->coordinates[1] % 2 == 0) {
            
            // Send SOUTH
            error = MPI_Send(last_row, grid->width, MPI_DOUBLE, heatsim->rank_south_peer, 4, heatsim->communicator);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed send south border", error);
                goto fail_exit;
            }

            // Then receive NORTH
            error = MPI_Recv(north_border, grid->width, MPI_DOUBLE, heatsim->rank_north_peer, 4, heatsim->communicator, MPI_STATUS_IGNORE);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed receive north border", error);
                goto fail_exit;
            }

            // Send NORTH
            error = MPI_Send(first_cell, grid->width, MPI_DOUBLE, heatsim->rank_north_peer, 5, heatsim->communicator);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed send north border", error);
                goto fail_exit;
            }

            // And receive SOUTH
            error = MPI_Recv(south_border, grid->width, MPI_DOUBLE, heatsim->rank_south_peer, 5, heatsim->communicator, MPI_STATUS_IGNORE);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed receive south border", error);
                goto fail_exit;
            }

        } else {
            
            // Receive NORTH
            error = MPI_Recv(north_border, grid->width, MPI_DOUBLE, heatsim->rank_north_peer, 4, heatsim->communicator, MPI_STATUS_IGNORE);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed receive north border", error);
                goto fail_exit;
            }

            // Then send SOUTH
            error = MPI_Send(last_row, grid->width, MPI_DOUBLE, heatsim->rank_south_peer, 4, heatsim->communicator);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed send south border", error);
                goto fail_exit;
            }

            // Receive SOUTH
            error = MPI_Recv(south_border, grid->width, MPI_DOUBLE, heatsim->rank_south_peer, 5, heatsim->communicator, MPI_STATUS_IGNORE);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed receive south border", error);
                goto fail_exit;
            }

            // Then send NORTH
            error = MPI_Send(first_cell, grid->width, MPI_DOUBLE, heatsim->rank_north_peer, 5, heatsim->communicator);

            if (error != MPI_SUCCESS)
            {
                LOG_ERROR_MPI("Failed send north border", error);
                goto fail_exit;
            }
        }
    } else {
        for (int i = 0; i < grid->width; i++) {
            north_border[i] = last_row[i];
            south_border[i] = first_cell[i];
        }
    }

    MPI_Type_free(&vector);

    return 0;
    
fail_exit:
    return -1;
}

int heatsim_send_result(heatsim_t* heatsim, grid_t* grid) {
    assert(grid->padding == 0);

    /*
     * TODO: Envoyer les données (`data`) du `grid` résultant au rang 0. Le
     *       `grid` n'a aucun rembourage (padding = 0);
     */

    int error = MPI_SUCCESS;

    error = MPI_Send(grid->data, grid->width * grid->height, MPI_DOUBLE, 0, 6, heatsim->communicator);

    if (error != MPI_SUCCESS)
    {
        LOG_ERROR_MPI("Failed send result", error);
        goto fail_exit;
    }

    return 0;

fail_exit:
    return -1;
}

int heatsim_receive_results(heatsim_t* heatsim, cart2d_t* cart) {
    /*
     * TODO: Recevoir toutes les `grid` des autres rangs. Aucune `grid`
     *       n'a de rembourage (padding = 0).
     *
     *       Utilisez `cart2d_get_grid` pour obtenir la `grid` à une coordonnée
     *       qui va recevoir le contenue (`data`) d'un autre noeud.
     */

    int error = MPI_SUCCESS;

    for (int i = 1; i < heatsim->rank_count; i++) {

        int coordinates[2];
        MPI_Cart_coords(heatsim->communicator, i, 2, coordinates);

        grid_t* destination = cart2d_get_grid(cart, coordinates[0], coordinates[1]);

        if (destination == NULL) {
            goto fail_exit;
        }

        error = MPI_Recv(destination->data, destination->width * destination->height, MPI_DOUBLE, i, 6, heatsim->communicator, MPI_STATUS_IGNORE);

        if (error != MPI_SUCCESS)
        {
            LOG_ERROR_MPI("Failed receive result", error);
            goto fail_exit;
        }
    }

    return 0;

fail_exit:
    return -1;
}
