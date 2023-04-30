package fun.scop.sat

case class SeqSatProblem() extends SatProblem {
  var clauses: IndexedSeq[Seq[Int]] = IndexedSeq.empty

  def addClause(lits: Seq[Int]): Unit = {
    nofClss += 1
    clauses = clauses :+ lits
  }

  def addComment(str: String): Unit = println("c " + str)
}

