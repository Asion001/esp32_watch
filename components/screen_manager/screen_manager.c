/**
 * @file screen_manager.c
 * @brief Unified screen management implementation
 */

#include "screen_manager.h"
#include "screen_navigation.h" #include "screen_navigation.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ScreenManager";

/**
 * @brief Navigation stack node
 */
typedef struct screen_stack_node
{
    lv_obj_t *screen;               /*!< Screen object */
    screen_anim_type_t anim_type;   /*!< Animation type for this screen */
    void (*hide_callback)(void);    /*!< Hide callback */
    struct screen_stack_node *next; /*!< Next node in stack */
} screen_stack_node_t;

/**
 * @brief Screen manager state
 */
typedef struct
{
    screen_stack_node_t *stack_top; /*!< Top of navigation stack */
    uint8_t stack_depth;            /*!< Current stack depth */
    bool initialized;               /*!< Initialization flag */
} screen_manager_state_t;

static screen_manager_state_t s_manager = {
    .stack_top = NULL,
    .stack_depth = 0,
    .initialized = false};

/**
 * @brief Push screen to navigation stack
 */
static esp_err_t push_screen(lv_obj_t *screen, screen_anim_type_t anim_type, void (*hide_callback)(void))
{
    if (s_manager.stack_depth >= CONFIG_SCREEN_MANAGER_MAX_STACK_DEPTH)
    {
        ESP_LOGE(TAG, "Navigation stack overflow (max depth: %d)", CONFIG_SCREEN_MANAGER_MAX_STACK_DEPTH);
        return ESP_FAIL;
    }

    screen_stack_node_t *node = (screen_stack_node_t *)malloc(sizeof(screen_stack_node_t));
    if (!node)
    {
        ESP_LOGE(TAG, "Failed to allocate stack node");
        return ESP_ERR_NO_MEM;
    }

    node->screen = screen;
    node->anim_type = anim_type;
    node->hide_callback = hide_callback;
    node->next = s_manager.stack_top;

    s_manager.stack_top = node;
    s_manager.stack_depth++;

    ESP_LOGD(TAG, "Pushed screen to stack (depth: %d)", s_manager.stack_depth);
    return ESP_OK;
}

/**
 * @brief Pop screen from navigation stack
 */
static screen_stack_node_t *pop_screen(void)
{
    if (!s_manager.stack_top)
    {
        ESP_LOGW(TAG, "Cannot pop from empty stack");
        return NULL;
    }

    screen_stack_node_t *node = s_manager.stack_top;
    s_manager.stack_top = node->next;
    s_manager.stack_depth--;

    ESP_LOGD(TAG, "Popped screen from stack (depth: %d)", s_manager.stack_depth);
    return node;
}

/**
 * @brief Remove screen from stack (anywhere in stack)
 */
static esp_err_t remove_from_stack(lv_obj_t *screen)
{
    if (!screen)
        return ESP_ERR_INVALID_ARG;

    screen_stack_node_t **current = &s_manager.stack_top;
    while (*current)
    {
        if ((*current)->screen == screen)
        {
            screen_stack_node_t *to_remove = *current;
            *current = to_remove->next;
            free(to_remove);
            s_manager.stack_depth--;
            ESP_LOGD(TAG, "Removed screen from stack (depth: %d)", s_manager.stack_depth);
            return ESP_OK;
        }
        current = &(*current)->next;
    }

    ESP_LOGW(TAG, "Screen not found in stack");
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Get animation type for screen
 */
static screen_anim_type_t get_screen_anim_type(lv_obj_t *screen)
{
    screen_stack_node_t *current = s_manager.stack_top;
    while (current)
    {
        if (current->screen == screen)
        {
            return current->anim_type;
        }
        current = current->next;
    }
    return SCREEN_ANIM_NONE;
}

/**
 * @brief Create gesture hint bar
 */
static void create_gesture_hint(lv_obj_t *parent)
{
    lv_obj_t *gesture_hint = lv_obj_create(parent);
    lv_obj_set_size(gesture_hint, 40, 4);
    lv_obj_align(gesture_hint, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_bg_color(gesture_hint, lv_color_hex(0x666666), 0);
    lv_obj_set_style_radius(gesture_hint, 2, 0);
    lv_obj_set_style_border_width(gesture_hint, 0, 0);
}

/**
 * @brief Create title label
 */
static void create_title(lv_obj_t *parent, const char *title_text)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
}

esp_err_t screen_manager_init(void)
{
    if (s_manager.initialized)
    {
        ESP_LOGW(TAG, "Screen manager already initialized");
        return ESP_OK;
    }

    s_manager.stack_top = NULL;
    s_manager.stack_depth = 0;
    s_manager.initialized = true;

    ESP_LOGI(TAG, "Screen manager initialized (max stack depth: %d)", CONFIG_SCREEN_MANAGER_MAX_STACK_DEPTH);
    return ESP_OK;
}

lv_obj_t *screen_manager_create(const screen_config_t *config)
{
    if (!s_manager.initialized)
    {
        ESP_LOGE(TAG, "Screen manager not initialized");
        return NULL;
    }

    if (!config)
    {
        ESP_LOGE(TAG, "Invalid config (NULL)");
        return NULL;
    }

    // Create screen container
    lv_obj_t *screen = lv_obj_create(NULL);
    if (!screen)
    {
        ESP_LOGE(TAG, "Failed to create screen object");
        return NULL;
    }

    // Style as black container with no border/padding
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    // Add gesture hint bar if requested
    if (config->has_gesture_hint)
    {
        create_gesture_hint(screen);
    }

    // Add title if provided
    if (config->title)
    {
        create_title(screen, config->title);
    }

    // Setup gestures based on animation type
    if (config->anim_type != SCREEN_ANIM_NONE && config->hide_callback)
    {
        lv_dir_t gesture_dir = (config->anim_type == SCREEN_ANIM_VERTICAL) ? LV_DIR_BOTTOM : LV_DIR_RIGHT;
        screen_nav_setup_gestures(screen, config->hide_callback, gesture_dir);
    }

    // Push to navigation stack
    if (push_screen(screen, config->anim_type, config->hide_callback) != ESP_OK)
    {
        lv_obj_delete(screen);
        return NULL;
    }

    ESP_LOGI(TAG, "Created screen: title='%s', anim=%d, hint=%d",
             config->title ? config->title : "none",
             config->anim_type,
             config->has_gesture_hint);

    return screen;
}

esp_err_t screen_manager_show(lv_obj_t *screen)
{
    if (!s_manager.initialized)
    {
        ESP_LOGE(TAG, "Screen manager not initialized");
        return ESP_FAIL;
    }

    if (!screen)
    {
        ESP_LOGE(TAG, "Invalid screen (NULL)");
        return ESP_ERR_INVALID_ARG;
    }

    // Get animation type for this screen
    screen_anim_type_t anim_type = get_screen_anim_type(screen);

    // Load screen with appropriate animation
    if (anim_type == SCREEN_ANIM_VERTICAL)
    {
        screen_nav_load_with_anim(screen, NULL); // Vertical (bottom-up)
    }
    else if (anim_type == SCREEN_ANIM_HORIZONTAL)
    {
        screen_nav_load_horizontal(screen, NULL); // Horizontal (right-to-left)
    }
    else
    {
        // No animation - instant load
        lv_scr_load(screen);
    }

    ESP_LOGD(TAG, "Showed screen with animation type %d", anim_type);
    return ESP_OK;
}

esp_err_t screen_manager_go_back(void)
{
    if (!s_manager.initialized)
    {
        ESP_LOGE(TAG, "Screen manager not initialized");
        return ESP_FAIL;
    }

    // Need at least 2 screens in stack to go back
    if (s_manager.stack_depth < 2)
    {
        ESP_LOGD(TAG, "At root screen, cannot go back");
        return ESP_OK; // Not an error, just no-op
    }

    // Pop current screen
    screen_stack_node_t *current_node = pop_screen();
    if (!current_node)
    {
        return ESP_FAIL;
    }

    // Get previous screen (now at top of stack)
    screen_stack_node_t *prev_node = s_manager.stack_top;
    if (!prev_node)
    {
        ESP_LOGE(TAG, "Stack inconsistency: no previous screen");
        free(current_node);
        return ESP_FAIL;
    }

    lv_obj_t *current_screen = current_node->screen;
    lv_obj_t *prev_screen = prev_node->screen;
    screen_anim_type_t anim_type = current_node->anim_type;

    // Call hide callback if provided
    if (current_node->hide_callback)
    {
        current_node->hide_callback();
    }

    // Animate back to previous screen
    if (anim_type == SCREEN_ANIM_VERTICAL)
    {
        screen_nav_go_back_with_anim(current_screen, prev_screen); // Vertical (top-down)
    }
    else if (anim_type == SCREEN_ANIM_HORIZONTAL)
    {
        screen_nav_go_back_horizontal(current_screen, prev_screen); // Horizontal (left-to-right)
    }
    else
    {
        lv_scr_load(prev_screen);
    }

    // Note: We don't destroy the current screen here - it remains in memory
    // for faster return. Could add destroy-on-hide flag in config if needed.

    free(current_node);
    ESP_LOGD(TAG, "Navigated back (stack depth: %d)", s_manager.stack_depth);

    return ESP_OK;
}

esp_err_t screen_manager_destroy(lv_obj_t *screen)
{
    if (!s_manager.initialized)
    {
        ESP_LOGE(TAG, "Screen manager not initialized");
        return ESP_FAIL;
    }

    if (!screen)
    {
        ESP_LOGE(TAG, "Invalid screen (NULL)");
        return ESP_ERR_INVALID_ARG;
    }

    // Remove from navigation stack
    remove_from_stack(screen);

    // Cleanup gestures (will be implemented in screen_nav)
    screen_nav_cleanup_gestures(screen);

    // Delete LVGL object (will trigger LV_EVENT_DELETE for any remaining cleanup)
    lv_obj_delete(screen);

    ESP_LOGD(TAG, "Destroyed screen");
    return ESP_OK;
}

lv_obj_t *screen_manager_get_current(void)
{
    if (!s_manager.initialized || !s_manager.stack_top)
    {
        return NULL;
    }
    return s_manager.stack_top->screen;
}

bool screen_manager_is_root(lv_obj_t *screen)
{
    if (!s_manager.initialized || !screen)
    {
        return false;
    }

    // Root screen is at bottom of stack
    screen_stack_node_t *current = s_manager.stack_top;
    if (!current)
        return false;

    // Traverse to bottom of stack
    while (current->next)
    {
        current = current->next;
    }

    return (current->screen == screen);
}
