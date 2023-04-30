package fun.scop.app.an.util

case class GlobalState(avs: Seq[AutomataValued]) {
  val avMap =
    (for {
      av <- avs
    } yield av.a -> av.v).toMap

  override def toString =
    avs.map { av => s"${av.v}" }.mkString("(", ",", ")")
}

object GlobalState {
  def fromMap(an: AutomataNetwork, map: Map[Automata, Int]) = {
    val asv = for {
      a <- an.automatas
      v = map(a)
    } yield AutomataValued(a, v)
    GlobalState(asv)
  }
}
