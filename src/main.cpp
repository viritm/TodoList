#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>
#include <sqlite3.h>
#include <iostream>
#include <ctime>

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)

/**
 *
 * \file main.cpp
 *
 * \brief Основной файл программы.
 *
 * Этот файл содержит основной код программы, включая функцию main().
 */

/**
 * @brief Структура, представляющая задачу в списке задач.
 */
struct Task
{

    std::string name;       ///< Название задачи.
    bool is_finished;       ///< Флаг, указывающий, выполнена ли задача.
    std::time_t time_added; ///< Время добавления задачи.

    /**
     * @brief Конструктор для инициализации задачи.
     * @param n Название задачи.
     * @param finished Указание, выполнена ли задача (true - выполнена, false - не выполнена).
     * @param added_time Время добавления задачи.
     */
    Task(const std::string &n, bool finished, std::time_t added_time) : name(n), is_finished(finished), time_added(added_time) {}
};

/**
 * \brief Добавляет новую задачу в список задач и сохраняет её в базе данных.
 *
 * @param task Название новой задачи.
 */
void add_task(const std::string &task);

/**
 * \brief Загружает задачи из базы данных, присваивая их вектору tasks.
 *
 * Загружаются только задачи с флагом is_finished, установленным в 0.
 */
void load_tasks_from_database();

/**
 * \brief Удаляет все завершенные задачи из вектора tasks и базы данных.
 *
 * Завершенные задачи определяются по флагу is_finished, установленному в true.
 * После удаления обновляет список завершенных задач.
 */
void delete_tasks();

/**
 * \brief Обновляет информацию о завершенности задач в базе данных на основе данных в векторе tasks.
 *
 * Для каждой задачи в векторе tasks обновляет запись в базе данных, устанавливая
 * значение поля task_finished в соответствии с флагом is_finished в задаче.
 *
 * @note Эта функция предполагает, что база данных уже открыта и будет закрыта в конце.
 */
void update_tasks_in_database();

/**
 * \brief Получает список завершенных задач из базы данных.
 *
 * Извлекает из базы данных все записи, у которых значение поля task_finished равно 1 (задачи завершены).
 * Создает и возвращает вектор Task, содержащий информацию о завершенных задачах.
 *
 * @return Вектор Task, содержащий информацию о завершенных задачах из базы данных.
 *         Если произошла ошибка при работе с базой данных, возвращается пустой вектор.
 */
std::vector<Task> get_finished_tasks();

/**
 * \brief Отображает список завершенных задач в пользовательском интерфейсе.
 *
 * Выводит в окно списка завершенных задач каждую завершенную задачу с их порядковыми номерами.
 *
 * @note Для правильного отображения текста задачи используется ImGui::TextWrapped(),
 *       что позволяет автоматически переносить текст на новую строку при необходимости.
 */
void render_finished_list();

/**
 * \brief Очищает базу данных от завершенных задач.
 *
 * Выполняет SQL-запрос для удаления всех завершенных задач (с флагом is_finished = 1) из таблицы tasks в базе данных.
 * После успешного выполнения выводит сообщение об успешной очистке.
 */
void clear_finished_tasks();

/**
 * @brief Создает базу данных и таблицу задач.
 *
 * Эта функция создает базу данных и таблицу для хранения задач. Если база данных уже существует,
 * она будет открыта, и если таблица уже существует, она не будет пересоздана.
 *
 * Структура таблицы 'tasks':
 * - 'task_name': TEXT, текстовое поле для хранения имени задачи.
 * - 'task_finished': INTEGER, целочисленное поле для хранения статуса задачи (0 - невыполнено, 1 - выполнено).
 */
void create_database_and_table();

/**
 * \brief Отображает список задач с возможностью отметки выполненных задач.
 *
 * Эта функция отображает список задач в области с прокруткой. Каждая задача представлена чекбоксом,
 * позволяющим отметить задачу как выполненную или невыполненную. Также рядом с чекбоксом выводится
 * порядковый номер задачи и текст задачи и временная метка. В случае изменения состояния чекбокса, состояние задачи
 * в векторе tasks также обновляется.
 */
void render_task_list();

std::vector<Task> tasks;          /**< Вектор задач, активных в данный момент. */
std::vector<Task> finished_tasks; /**< Вектор завершенных задач. */

std::string removeQuotes(const std::string &input)
{
    if (input.size() >= 2 && input.front() == '"' && input.back() == '"')
    {
        return input.substr(1, input.size() - 2);
    }
    return input;
}

int main()
{
    if (!glfwInit())
    {
        return -1;
    }

    GLFWwindow *window = glfwCreateWindow(1024, 768, "Список задач", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    std::string projectRootDir = STRINGIZE(PROJECT_ROOT_DIR);
    std::string fontPath = removeQuotes(projectRootDir) + "/fonts/arial.ttf";
    ImFont *font = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16, NULL, io.Fonts->GetGlyphRangesCyrillic());
    if (!font)
    {
        std::cerr << "Failed to load Cyrillic font!" << std::endl;
    }

    char task_input[256] = "";
    bool show_finished_list = false;

    load_tasks_from_database();
    finished_tasks = {get_finished_tasks()};

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    while (!glfwWindowShouldClose(window))
    {

        create_database_and_table();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(800, 600));
        if (ImGui::Begin("Список задач", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            render_task_list();
            if (ImGui::Button("Удалить выделенные"))
            {
                delete_tasks();
            }
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Поле для ввода");
            ImGui::InputText(" ", task_input, sizeof(task_input));
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.0f, 1.0f));
            if (ImGui::Button("Добавить", ImVec2(76, 23)) || (io.KeysDown[ImGuiKey_Enter] && io.KeyMods == 0))
            {
                if (strlen(task_input) > 0)
                {
                    add_task(task_input);
                    memset(task_input, 0, sizeof(task_input));
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::PopStyleColor();

            if (ImGui::Button("Показать выполнненые задачи"))
            {
                show_finished_list = !show_finished_list;
                ImGui::SameLine();
                ImGui::Spacing();
            }

            if (show_finished_list)
            {
                render_finished_list();
                ImGui::SameLine();
                ImGui::Spacing();
                if (ImGui::Button("Очистить список"))
                {
                    finished_tasks.clear();
                    clear_finished_tasks();
                }
            }
        }
        ImGui::End();

        ImGui::Render();

        glfwMakeContextCurrent(window);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

void create_database_and_table()
{
    const char *db_file = "todo_list.db";

    sqlite3 *db;

    int rc = sqlite3_open(db_file, &db);

    if (rc != SQLITE_OK)
    {
        // fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return;
    }

    const char *create_table_query = "CREATE TABLE IF NOT EXISTS tasks ("
                                     "task_name TEXT NOT NULL,"
                                     "task_finished INTEGER NOT NULL,"
                                     "time_added TEXT NOT NULL"
                                     ");";

    sqlite3_stmt *stmt;

    rc = sqlite3_prepare_v2(db, create_table_query, -1, &stmt, 0);

    if (rc != SQLITE_OK)
    {
        // fprintf(stderr, "Failed to prepare query: %s\n", sqlite3_errmsg(db));
        return;
    }

    rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);

    sqlite3_close(db);
}

void add_task(const std::string &task)
{

    if (!task.empty())
    {
        Task new_task(task, false, std::time(nullptr));
        tasks.push_back(new_task);

        sqlite3 *db;
        int result = sqlite3_open("todo_list.db", &db);
        if (result == SQLITE_OK)
        {
            std::string sql = "INSERT INTO tasks (task_name, task_finished, time_added) VALUES ('" + task + "', 0, " +
                              std::to_string(new_task.time_added) + ");";

            result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
            if (result != SQLITE_OK)
            {
                std::cerr << "Error inserting task into database: " << sqlite3_errmsg(db) << std::endl;
            }

            sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);

            // Закрываем базу данных
            sqlite3_close(db);
        }
        else
        {
            std::cerr << "Error opening database!" << std::endl;
        }
    }
}

void load_tasks_from_database()
{
    sqlite3 *db;
    int result = sqlite3_open("todo_list.db", &db);

    if (result == SQLITE_OK)
    {
        const char *select_query = "SELECT task_name, task_finished, time_added FROM tasks WHERE task_finished = 0;";
        sqlite3_stmt *stmt;

        result = sqlite3_prepare_v2(db, select_query, -1, &stmt, nullptr);
        if (result != SQLITE_OK)
        {
            // std::cerr << "Failed to prepare query: " << sqlite3_errmsg(db) << std::endl;
            return;
        }

        tasks.clear();

        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *task_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            bool is_finished = static_cast<bool>(sqlite3_column_int(stmt, 1));
            std::time_t time_added = static_cast<std::time_t>(sqlite3_column_int(stmt, 2));

            tasks.push_back(Task(task_name, is_finished, time_added));
        }

        sqlite3_finalize(stmt);

        sqlite3_close(db);
    }
    else
    {
        std::cerr << "Error opening database!" << std::endl;
    }
}

void render_finished_list()
{
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Завершенные задачи");
    ImGui::BeginChild("FinishedTaskList", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);

    for (int i = 0; i < finished_tasks.size(); i++)
    {
        std::string task_text = finished_tasks[i].name;
        ImGui::Text("%d.", i + 1);
        ImGui::SameLine();
        ImGui::TextWrapped("%s", task_text.c_str());
    }

    ImGui::EndChild();
}

void render_task_list()
{
    // Область с прокруткой для списка задач
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Актуальные задачи");
    ImGui::BeginChild("TaskList", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);

    for (int i = 0; i < tasks.size(); i++)
    {

        std::string task_text = tasks[i].name;

        std::tm *local_tm = std::localtime(&tasks[i].time_added);
        char time_buffer[80];
        std::strftime(time_buffer, sizeof(time_buffer), "%H:%M %d.%m.%Y", local_tm);
        std::string time_text = time_buffer;

        bool is_checked = tasks[i].is_finished;
        if (ImGui::Checkbox(("##Checkbox" + std::to_string(i)).c_str(), &is_checked))
        {
            tasks[i].is_finished = is_checked;
        }
        ImGui::SameLine();
        ImGui::Text("%d.", i + 1);
        ImGui::SameLine();
        ImGui::TextWrapped("%s", task_text.c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 120);
        ImGui::Text("%s", time_text.c_str());
    }

    ImGui::EndChild();
}

void update_tasks_in_database()
{
    sqlite3 *db;
    int result = sqlite3_open("todo_list.db", &db);

    if (result == SQLITE_OK)
    {

        for (const auto &task : tasks)
        {
            const char *update_query = "UPDATE tasks SET task_finished = ? WHERE task_name = ?;";
            sqlite3_stmt *stmt;

            result = sqlite3_prepare_v2(db, update_query, -1, &stmt, nullptr);
            if (result != SQLITE_OK)
            {
                // std::cerr << "Failed to prepare query: " << sqlite3_errmsg(db) << std::endl;
                return;
            }

            int is_finished = task.is_finished ? 1 : 0;

            const char *task_name = task.name.c_str();

            sqlite3_bind_int(stmt, 1, is_finished);
            sqlite3_bind_text(stmt, 2, task_name, -1, SQLITE_STATIC);

            // Выполняем запрос
            result = sqlite3_step(stmt);

            sqlite3_finalize(stmt);

            if (result != SQLITE_DONE)
            {
                std::cerr << "Failed to update task in database: " << sqlite3_errmsg(db) << std::endl;
                return;
            }
        }

        sqlite3_close(db);
    }
    else
    {
        std::cerr << "Error opening database!" << std::endl;
    }
}

void clear_finished_tasks()
{
    const char *db_file = "todo_list.db";
    sqlite3 *db;

    int rc = sqlite3_open(db_file, &db);

    if (rc != SQLITE_OK)
    {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    const char *clear_finished_tasks_query = "DELETE FROM tasks WHERE task_finished = 1;";

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, clear_finished_tasks_query, -1, &stmt, 0);

    if (rc != SQLITE_OK)
    {
        // std::cerr << "Failed to prepare query: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        std::cerr << "Execution failed: " << sqlite3_errmsg(db) << std::endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::cout << "Finished tasks cleared successfully." << std::endl;
}

void delete_tasks()
{
    update_tasks_in_database();

    auto it = tasks.begin();
    while (it != tasks.end())
    {
        if (it->is_finished)
        {
            it = tasks.erase(it);
        }
        else
        {
            ++it;
        }
    }

    finished_tasks = get_finished_tasks();
}

std::vector<Task> get_finished_tasks()
{
    const char *db_file = "todo_list.db";

    sqlite3 *db;
    int rc = sqlite3_open(db_file, &db);

    std::vector<Task> finished_tasks;

    if (rc != SQLITE_OK)
    {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return finished_tasks;
    }

    const char *select_query = "SELECT task_name, task_finished, time_added FROM tasks WHERE task_finished = 1;";

    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, select_query, -1, &stmt, 0);

    if (rc != SQLITE_OK)
    {
        // std::cerr << "Failed to prepare query: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return finished_tasks;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        std::string task_name = std::string(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
        bool task_finished = sqlite3_column_int(stmt, 1) == 1;
        std::time_t time_added = static_cast<std::time_t>(sqlite3_column_int(stmt, 2));
        Task task(task_name, task_finished, time_added);
        finished_tasks.push_back(task);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return finished_tasks;
}
