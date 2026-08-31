#include "repositories/CoachTraineeRepository.hpp"

namespace fitplan::repositories {

namespace {

// Column order: 0 id  1 email  2 password_hash  3 role  4 display_name
//   5 created_at  - matches the SELECT in list_trainees().
models::User row_to_user(SQLite::Statement& stmt) {
    models::User u;
    u.id = stmt.getColumn(0).getInt64();
    u.email = stmt.getColumn(1).getString();
    u.password_hash = stmt.getColumn(2).getString();
    u.role = stmt.getColumn(3).getString();
    u.display_name = stmt.getColumn(4).getString();
    u.created_at = stmt.getColumn(5).getString();
    return u;
}

}  // namespace

bool CoachTraineeRepository::link(std::int64_t coach_id, std::int64_t trainee_id) {
    SQLite::Statement stmt(
        db_,
        "INSERT OR IGNORE INTO coach_trainees (coach_id, trainee_id) "
        "VALUES (?, ?)");
    stmt.bind(1, coach_id);
    stmt.bind(2, trainee_id);
    return stmt.exec() > 0;  // 0 rows => the pair was already there
}

bool CoachTraineeRepository::is_linked(std::int64_t coach_id,
                                      std::int64_t trainee_id) {
    SQLite::Statement stmt(
        db_,
        "SELECT 1 FROM coach_trainees WHERE coach_id = ? AND trainee_id = ? "
        "LIMIT 1");
    stmt.bind(1, coach_id);
    stmt.bind(2, trainee_id);
    return stmt.executeStep();
}

bool CoachTraineeRepository::unlink(std::int64_t coach_id,
                                   std::int64_t trainee_id) {
    SQLite::Statement stmt(
        db_,
        "DELETE FROM coach_trainees WHERE coach_id = ? AND trainee_id = ?");
    stmt.bind(1, coach_id);
    stmt.bind(2, trainee_id);
    return stmt.exec() > 0;  // 0 rows => the pair was not on the roster
}

std::vector<models::User> CoachTraineeRepository::list_trainees(
    std::int64_t coach_id) {
    SQLite::Statement stmt(
        db_,
        "SELECT u.id, u.email, u.password_hash, u.role, u.display_name, "
        "       u.created_at "
        "FROM coach_trainees ct "
        "JOIN users u ON u.id = ct.trainee_id "
        "WHERE ct.coach_id = ? "
        "ORDER BY ct.created_at DESC, u.id DESC");
    stmt.bind(1, coach_id);

    std::vector<models::User> result;
    while (stmt.executeStep()) {
        result.push_back(row_to_user(stmt));
    }
    return result;
}

}  // namespace fitplan::repositories
