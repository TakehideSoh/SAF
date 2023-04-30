package fun.scop.sat

trait SatProblem {
  var nofVars = 0
  var nofClss = 0

  def addAllClauses(clauses: Seq[Seq[Int]]) = {
    for (clause <- clauses)
      addClause(clause)
  }

  def addClause(lits: Seq[Int]): Unit

  def addComment(str: String): Unit
}
